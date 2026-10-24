// BSD sockets backend: getaddrinfo, non-blocking connect with timeout, poll() I/O.
#include "net_posix.hpp"
#include "log.hpp"
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace wx {

struct Sock { int fd; int timeout_ms; };
static Sock g_sock;   // one connection at a time (a CLI tool)

static long sock_read(Io &io, void *buf, size_t len)
{
	Sock *s = static_cast<Sock *>(io.ctx);
	for (;;) {
		struct pollfd p = { s->fd, POLLIN, 0 };
		int r = poll(&p, 1, s->timeout_ms);
		if (r == 0) { io.err = Err::Timeout; io.detail = ETIMEDOUT; io.msg = "timed out waiting for the server"; return -1; }
		if (r < 0) {
			if (errno == EINTR) continue;
			io.err = Err::Io; io.detail = errno; io.msg = "network I/O error"; return -1;
		}
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
		struct pollfd pf = { s->fd, POLLOUT, 0 };
		int r = poll(&pf, 1, s->timeout_ms);
		if (r == 0) { io.err = Err::Timeout; io.detail = ETIMEDOUT; io.msg = "timed out waiting for the server"; return -1; }
		if (r < 0) {
			if (errno == EINTR) continue;
			io.err = Err::Io; io.detail = errno; io.msg = "network I/O error"; return -1;
		}
		ssize_t n = write(s->fd, p + done, len - done);
		if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
		if (n < 0) { io.err = Err::Io; io.detail = errno; io.msg = "network I/O error"; return -1; }
		done += static_cast<size_t>(n);
	}
	return static_cast<long>(done);
}

static Status posix_connect(Net &, const char *host, const char *port, int timeout_s, Io *out)
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
	int fd = -1, lasterr = 0;
	bool timed_out = false;
	for (struct addrinfo *a = res; a; a = a->ai_next) {
		fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
		if (fd < 0) { lasterr = errno; continue; }
		int fl = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, fl | O_NONBLOCK);
		int r = connect(fd, a->ai_addr, a->ai_addrlen);
		if (r < 0 && errno == EINPROGRESS) {
			struct pollfd p = { fd, POLLOUT, 0 };
			int pr;
			do { pr = poll(&p, 1, timeout_s * 1000); } while (pr < 0 && errno == EINTR);
			if (pr == 0) { timed_out = true; lasterr = ETIMEDOUT; r = -1; }
			else if (pr < 0) { lasterr = errno; r = -1; }
			else {
				int soerr = 0;
				socklen_t sl = sizeof soerr;
				getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl);
				if (soerr) { lasterr = soerr; r = -1; } else r = 0;
			}
		} else if (r < 0) {
			lasterr = errno;
		}
		if (r == 0) { fcntl(fd, F_SETFL, fl); break; }
		close(fd);
		fd = -1;
	}
	freeaddrinfo(res);
	if (fd < 0) {
		return timed_out ? make(Err::Timeout, "connect timed out", lasterr)
		                 : make(Err::Connect, "cannot connect", lasterr);
	}
	g_sock.fd = fd;
	g_sock.timeout_ms = timeout_s * 1000;
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

static void posix_sleep(Net &, long ms)
{
	struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
	nanosleep(&ts, nullptr);
}

static long posix_now(Net &) { return static_cast<long>(time(nullptr)); }

void posix_net_init(Net *net)
{
	net->connect = posix_connect;
	net->close = posix_close;
	net->sleep_ms = posix_sleep;
	net->now = posix_now;
}

} // namespace wx
