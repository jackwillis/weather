// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// BSD sockets backend: getaddrinfo, non-blocking connect, poll() I/O in 50 ms slices so
// Net::yield can run (and cancel). Known limit: getaddrinfo() itself cannot be interrupted.
#include "net_posix.hpp"
#include "core/log.hpp"
#include <cerrno>
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
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return static_cast<long long>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

enum { WaitReady = 1, WaitTimeout = 0, WaitError = -1, WaitCancelled = -2 };

// Wait for fd; slices only when there is a yield hook to run.
static int wait_fd(Net *net, int fd, short ev, int timeout_ms)
{
	long long end = mono_ms() + timeout_ms;
	for (;;) {
		long long left = end - mono_ms();
		if (left < 0) left = 0;
		bool slicing = net && net->yield;
		int slice = (slicing && left > kSliceMs) ? kSliceMs : static_cast<int>(left);
		struct pollfd p = { fd, ev, 0 };
		int r = poll(&p, 1, slice);
		if (r > 0) return WaitReady;
		if (r < 0 && errno != EINTR) return WaitError;
		if (slicing && !net->yield(*net)) return WaitCancelled;
		if (mono_ms() >= end) return WaitTimeout;
	}
}

static void fail_wait(Io &io, int w)
{
	if (w == WaitTimeout) { io.err = Err::Timeout; io.detail = ETIMEDOUT; io.msg = "timed out waiting for the server"; }
	else if (w == WaitCancelled) { io.err = Err::Cancelled; io.detail = 0; io.msg = "cancelled"; }
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

static Status posix_connect(Net &net, const char *host, const char *port, int timeout_s, Io *out)
{
	struct addrinfo hints, *res = nullptr;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	int gai = getaddrinfo(host, port, &hints, &res);
	if (gai != 0) {
		logf(LogDebug, "getaddrinfo(%s): %s", host, gai_strerror(gai));
		return make(Err::Dns, "cannot resolve host name", gai);
	}
	int fd = -1, lasterr = 0, why = WaitReady;
	for (struct addrinfo *a = res; a; a = a->ai_next) {
		fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
		if (fd < 0) { lasterr = errno; continue; }
		int fl = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, fl | O_NONBLOCK);
		int r = connect(fd, a->ai_addr, a->ai_addrlen);
		if (r < 0 && errno == EINPROGRESS) {
			int w = wait_fd(&net, fd, POLLOUT, timeout_s * 1000);
			if (w == WaitReady) {
				int soerr = 0;
				socklen_t sl = sizeof soerr;
				getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl);
				if (soerr) { lasterr = soerr; r = -1; } else r = 0;
			} else {
				why = w;
				lasterr = w == WaitTimeout ? ETIMEDOUT : errno;
				r = -1;
			}
		} else if (r < 0) {
			lasterr = errno;
		}
		if (r == 0) { fcntl(fd, F_SETFL, fl); why = WaitReady; break; }
		close(fd);
		fd = -1;
		if (why == WaitCancelled) break;
	}
	freeaddrinfo(res);
	if (fd < 0) {
		if (why == WaitCancelled) return make(Err::Cancelled, "cancelled while connecting");
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
	long long end = mono_ms() + ms;
	for (;;) {
		long long left = end - mono_ms();
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

void posix_net_init(Net *net)
{
	net->connect = posix_connect;
	net->close = posix_close;
	net->sleep_ms = posix_sleep;
	net->now = posix_now;
	net->entropy = posix_entropy;
}

} // namespace wx
