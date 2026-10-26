// BSD sockets backend: getaddrinfo, non-blocking connect, poll() I/O in 50 ms slices so
// Net::yield can run (and cancel). Known limit: getaddrinfo() itself cannot be interrupted.
#include "net_posix.hpp"
#include "core/log.hpp"
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace wx {

constexpr int kSliceMs = 50;

struct Sock { int fd; int timeout_ms; Net *net; };
static Sock g_sock;   // one connection at a time (a CLI/window app)

static long long mono_ms()
{
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return -1;
	return static_cast<long long>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

enum { WaitReady = 1, WaitTimeout = 0, WaitError = -1, WaitCancelled = -2, WaitClock = -3 };

// Wait for fd; slices only when there is a yield hook to run.
static int wait_fd(Net *net, int fd, short ev, int timeout_ms)
{
	long long start = mono_ms();
	if (start < 0) return WaitClock;
	for (;;) {
		long long now = mono_ms();
		if (now < start) return WaitClock;
		long long left = timeout_ms - (now - start);
		if (left < 0) left = 0;
		bool slicing = net && net->yield;
		int slice = (slicing && left > kSliceMs) ? kSliceMs : static_cast<int>(left);
		struct pollfd p = { fd, ev, 0 };
		int r = poll(&p, 1, slice);
		if (r > 0) return WaitReady;
		if (r < 0 && errno != EINTR) return WaitError;
		if (slicing && !net->yield(*net)) return WaitCancelled;
		now = mono_ms();
		if (now < start) return WaitClock;
		if (now - start >= timeout_ms) return WaitTimeout;
	}
}

static void fail_wait(Io &io, int w)
{
	if (w == WaitTimeout) { io.err = Err::Timeout; io.detail = ETIMEDOUT; io.msg = "timed out waiting for the server"; }
	else if (w == WaitCancelled) { io.err = Err::Cancelled; io.detail = 0; io.msg = "cancelled"; }
	else if (w == WaitClock) { io.err = Err::Clock; io.detail = 0; io.msg = "monotonic clock unavailable or moved backwards"; }
	else { io.err = Err::Io; io.detail = errno; io.msg = "network I/O error"; }
}

static long sock_read(Io &io, void *buf, size_t len)
{
	Sock *s = static_cast<Sock *>(io.ctx);
	for (;;) {
		int w = wait_fd(s->net, s->fd, POLLIN, s->timeout_ms);
		if (w != WaitReady) { fail_wait(io, w); return -1; }
		ssize_t n = read(s->fd, buf, len);
		if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
		if (n < 0) { io.err = Err::Io; io.detail = errno; io.msg = "network I/O error"; return -1; }
		return static_cast<long>(n);
	}
}

static long sock_write(Io &io, const void *buf, size_t len)
{
	Sock *s = static_cast<Sock *>(io.ctx);
	const char *p = static_cast<const char *>(buf);
	size_t done = 0;
	while (done < len) {
		int w = wait_fd(s->net, s->fd, POLLOUT, s->timeout_ms);
		if (w != WaitReady) { fail_wait(io, w); return -1; }
		ssize_t n = write(s->fd, p + done, len - done);
		if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
		if (n < 0) { io.err = Err::Io; io.detail = errno; io.msg = "network I/O error"; return -1; }
		done += static_cast<size_t>(n);
	}
	return static_cast<long>(done);
}

static long long posix_monotonic(Net &);

// One budget for lookup elapsed time and every address, without adding a
// deadline to a timestamp or overflowing a 32-bit millisecond count.
static int connect_left(Net &net, long long start, int budget)
{
	long long now = posix_monotonic(net);
	if (now < start) return -1;
	long long elapsed = now - start;
	return elapsed >= budget ? 0 : budget - static_cast<int>(elapsed);
}

static int wait_connect(Net &net, int fd, long long start, int budget)
{
	for (;;) {
		if (net.yield && !net.yield(net)) return WaitCancelled;
		int left = connect_left(net, start, budget);
		if (left < 0) return WaitClock;
		if (left == 0) return WaitTimeout;
		int slice = net.yield && left > kSliceMs ? kSliceMs : left;
		struct pollfd p = {fd, POLLOUT, 0};
		int r = poll(&p, 1, slice);
		int poll_error = errno;
		left = connect_left(net, start, budget);
		if (left < 0) return WaitClock;
		if (left == 0) return WaitTimeout;
		if (r > 0) return WaitReady;
		if (r < 0 && poll_error != EINTR) { errno = poll_error; return WaitError; }
	}
}

static Status posix_connect(Net &net, const char *host, const char *port, int timeout_s, Io *out)
{
	if (timeout_s <= 0 || timeout_s > INT_MAX / 1000) return make(Err::Config, "invalid connect timeout");
	const int budget = timeout_s * 1000;
	const long long start = posix_monotonic(net);
	if (start < 0) return make(Err::Clock, "monotonic clock unavailable");
	if (net.yield && !net.yield(net)) return make(Err::Cancelled, "cancelled before host lookup");
	struct addrinfo hints, *res = nullptr;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	int gai = getaddrinfo(host, port, &hints, &res);
	int left = connect_left(net, start, budget);
	bool cancelled = net.yield && !net.yield(net);
	if (cancelled || left <= 0) {
		if (gai == 0) freeaddrinfo(res);
		if (cancelled) return make(Err::Cancelled, "cancelled after host lookup");
		return left < 0 ? make(Err::Clock, "monotonic clock unavailable or moved backwards")
		                : make(Err::Timeout, "name lookup and connect timed out", ETIMEDOUT);
	}
	if (gai != 0) {
		logf(LogDebug, "getaddrinfo(%s): %s", host, gai_strerror(gai));
		return make(Err::Dns, "cannot resolve host name", gai);
	}
	int fd = -1, lasterr = 0, why = WaitReady;
	for (struct addrinfo *a = res; a; a = a->ai_next) {
		if (net.yield && !net.yield(net)) { why = WaitCancelled; break; }
		left = connect_left(net, start, budget);
		if (left <= 0) { why = left < 0 ? WaitClock : WaitTimeout; lasterr = ETIMEDOUT; break; }
		fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
		if (fd < 0) { lasterr = errno; continue; }
		int fl = fcntl(fd, F_GETFL, 0);
		if (fl < 0 || fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0) {
			lasterr = errno; close(fd); fd = -1; continue;
		}
		int r = connect(fd, a->ai_addr, a->ai_addrlen);
		if (r < 0 && errno == EINPROGRESS) {
			int w = wait_connect(net, fd, start, budget);
			if (w == WaitReady) {
				int soerr = 0;
				socklen_t sl = sizeof soerr;
				if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl) != 0) { lasterr = errno; r = -1; }
				else if (soerr) { lasterr = soerr; r = -1; } else r = 0;
			} else {
				why = w;
				lasterr = w == WaitTimeout ? ETIMEDOUT : errno;
				r = -1;
			}
		} else if (r < 0) {
			lasterr = errno;
		}
		left = connect_left(net, start, budget);
		if (why != WaitCancelled && left <= 0) {
			why = left < 0 ? WaitClock : WaitTimeout; lasterr = ETIMEDOUT;
		}
		if (r == 0 && left > 0) {
			if (fcntl(fd, F_SETFL, fl) == 0) { why = WaitReady; break; }
			lasterr = errno;
		}
		close(fd);
		fd = -1;
		if (why == WaitCancelled || why == WaitTimeout || why == WaitClock) break;
	}
	freeaddrinfo(res);
	if (fd < 0) {
		if (why == WaitCancelled) return make(Err::Cancelled, "cancelled while connecting");
		if (why == WaitClock) return make(Err::Clock, "monotonic clock unavailable or moved backwards");
		return why == WaitTimeout ? make(Err::Timeout, "connect timed out", lasterr)
		                          : make(Err::Connect, "cannot connect", lasterr);
	}
	g_sock.fd = fd;
	g_sock.timeout_ms = timeout_s * 1000;
	g_sock.net = &net;
	out->ctx = &g_sock;
	out->read = sock_read;
	out->write = sock_write;
	out->err = Err::Ok;
	out->detail = 0;
	out->msg = nullptr;
	return okay();
}

static void posix_close(Net &, Io &io)
{
	Sock *s = static_cast<Sock *>(io.ctx);
	if (s && s->fd >= 0) { close(s->fd); s->fd = -1; }
}

static bool posix_sleep(Net &net, long ms)
{
	if (ms <= 0) return true;
	long long start = mono_ms();
	if (start < 0) return false;
	for (;;) {
		long long now = mono_ms();
		if (now < start) return false;
		long long left = ms - (now - start);
		if (left <= 0) return true;
		long step = left > kSliceMs ? kSliceMs : static_cast<long>(left);
		struct timespec ts = { step / 1000, (step % 1000) * 1000000L };
		nanosleep(&ts, nullptr);
		if (net.yield && !net.yield(net)) return false;
	}
}

// Test hook: WEATHER_NOW=<unix seconds> replaces the clock, to prove that
// certificate checking really uses this seam (e.g. year 2100 => "expired").
static long long posix_now(Net &)
{
	if (const char *fake = getenv("WEATHER_NOW")) return atoll(fake);
	return static_cast<long long>(time(nullptr));
}

static bool posix_entropy(Net &, void *buf, size_t len)
{
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0) return false;
	size_t got = 0;
	while (got < len) {
		ssize_t n = read(fd, static_cast<char *>(buf) + got, len - got);
		if (n < 0 && errno == EINTR) continue;
		if (n <= 0) break;
		got += static_cast<size_t>(n);
	}
	close(fd);
	return got == len;
}

static long long posix_monotonic(Net &)
{
	return mono_ms();
}

void posix_net_init(Net *net)
{
	net->connect = posix_connect;
	net->close = posix_close;
	net->sleep_ms = posix_sleep;
	net->now = posix_now;
	net->monotonic_ms = posix_monotonic;
	net->entropy = posix_entropy;
}

} // namespace wx
