// Compile the actual backend with deterministic syscall boundaries, not a copy
// of the address loop. No real DNS, sockets, wall-clock waits or interposition.
#include "platform/posix/net_posix.hpp"
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
#include <cstdio>
#include <cstdarg>
namespace {
long long ticks, dns_delay, connect_delay;
int connects, closes, polls, frees, dns_calls, immediate_failures;
bool ready, fail_on_poll, clock_errno_noise;
int poll_mode, socket_error, dns_error, flag_failure;
bool option_failure;
int clock_calls, clock_fail_at, clock_back_at, yield_calls, cancel_at;
addrinfo addresses[3];
sockaddr address;
int test_clock(clockid_t, timespec *ts) {
	clock_calls++;
	if (clock_errno_noise) errno = ERANGE;
	if (clock_fail_at && clock_calls >= clock_fail_at) { errno = EIO; return -1; }
	long long now = clock_back_at && clock_calls >= clock_back_at ? ticks - 10000 : ticks;
	ts->tv_sec = now / 1000; ts->tv_nsec = now % 1000 * 1000000; return 0;
}
int test_dns(const char *, const char *, const addrinfo *, addrinfo **out) {
	dns_calls++; ticks += dns_delay;
	if (dns_error) { *out = nullptr; return dns_error; }
	for (int i = 0; i < 3; i++) {
		addresses[i] = {}; addresses[i].ai_family = AF_INET; addresses[i].ai_socktype = SOCK_STREAM;
		addresses[i].ai_addr = &address; addresses[i].ai_addrlen = sizeof address;
		addresses[i].ai_next = i < 2 ? &addresses[i + 1] : nullptr;
	}
	*out = addresses; return 0;
}
void test_free(addrinfo *) { frees++; }
int test_socket(int, int, int) { return 123; }
int test_fcntl(int, int cmd, ...) {
	if (cmd == F_GETFL) { if (flag_failure == 1) { errno = EIO; return -1; } return 0; }
	va_list args; va_start(args, cmd); int flags = va_arg(args, int); va_end(args);
	if ((flag_failure == 2 && flags != 0) || (flag_failure == 3 && flags == 0)) { errno = EPERM; return -1; }
	return 0;
}
int test_connect(int, const sockaddr *, socklen_t) {
	connects++; ticks += connect_delay;
	if (connects <= immediate_failures) { errno = ECONNREFUSED; return -1; }
	if (ready) return 0;
	errno = EINPROGRESS; return -1;
}
int test_poll(pollfd *, nfds_t, int timeout) {
	polls++; ticks += poll_mode ? (timeout < 20 ? timeout : 20) : timeout;
	if (fail_on_poll) clock_fail_at = clock_calls + 1;
	if (poll_mode == 1) return 1;
	if (poll_mode >= 2) { errno = poll_mode == 2 ? EINTR : EIO; return -1; }
	return 0;
}
int test_option(int, int, int, void *value, socklen_t *) { if (option_failure) { errno = ENOTSOCK; return -1; } *static_cast<int *>(value) = socket_error; return 0; }
int test_close(int) { closes++; return 0; }
void reset() { ticks = dns_delay = connect_delay = 0; connects = closes = polls = frees = dns_calls = immediate_failures = 0; ready = fail_on_poll = clock_errno_noise = option_failure = false; clock_calls = clock_fail_at = clock_back_at = yield_calls = cancel_at = poll_mode = socket_error = dns_error = flag_failure = 0; }
}
#define clock_gettime(...) test_clock(__VA_ARGS__)
#define getaddrinfo(...) test_dns(__VA_ARGS__)
#define freeaddrinfo(...) test_free(__VA_ARGS__)
#define socket(...) test_socket(__VA_ARGS__)
#define fcntl(...) test_fcntl(__VA_ARGS__)
#define connect(...) test_connect(__VA_ARGS__)
#define poll(...) test_poll(__VA_ARGS__)
#define close(...) test_close(__VA_ARGS__)
#define getsockopt(...) test_option(__VA_ARGS__)
#include "../src/platform/posix/net_posix.cpp"
#undef clock_gettime
#undef getaddrinfo
#undef freeaddrinfo
#undef socket
#undef fcntl
#undef connect
#undef poll
#undef close
#undef getsockopt
static bool test_yield(wx::Net &) { return ++yield_calls < cancel_at; }
static int checks, failures;
static void check(bool ok, const char *name) { checks++; if (!ok) { failures++; printf("FAIL connect budget: %s\n", name); } }
int main()
{
	wx::log_init(wx::LogError, false, "connect-budget");
	wx::Net net; wx::posix_net_init(&net); wx::Io out;
	reset();
	wx::Status status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Timeout && ticks == 1000, "all addresses share one second");
	check(connects == 1 && closes == 1 && frees == 1, "stop after exhausted budget and release resources");
	reset(); dns_delay = 600;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Timeout && ticks == 1000, "lookup elapsed time is charged");
	reset(); dns_delay = 1500;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Timeout && connects == 0 && closes == 0 && frees == 1, "expired lookup opens no socket");
	reset(); immediate_failures = 1; connect_delay = 300;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Timeout && ticks == 1000 && connects == 2, "immediate failures consume the shared budget");
	reset(); ready = true; dns_delay = 200; immediate_failures = 1; connect_delay = 100;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.ok() && ticks == 400 && connects == 2 && closes == 1 && frees == 1, "fallback can succeed inside budget");
	if (status.ok()) net.close(net, out);
	check(closes == 2, "successful socket closes normally");
	reset(); ready = true; connect_delay = 1100;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Timeout && closes == 1, "late immediate success is rejected");
	reset(); immediate_failures = 3; connect_delay = 400;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Timeout && closes == 3 && frees == 1, "last immediate failure cannot hide expiry");
	reset(); clock_fail_at = 1;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Clock && dns_calls == 0, "missing clock fails before lookup");
	reset(); ticks = 100000; clock_back_at = 3;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Clock && connects == 0 && frees == 1, "backward clock after lookup fails closed");
	reset(); fail_on_poll = true; net.yield = test_yield; cancel_at = 100;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Clock && closes == 1 && frees == 1, "missing clock during wait closes socket");
	reset(); cancel_at = 1;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Cancelled && dns_calls == 0, "cancel before lookup");
	reset(); cancel_at = 2; dns_delay = 1500;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Cancelled && connects == 0 && frees == 1, "cancel wins after delayed lookup");
	reset(); cancel_at = 5;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Cancelled && ticks <= 100 && closes == 1 && frees == 1, "cancel while waiting for address");
	net.yield = nullptr;
	reset();
	status = net.connect(net, "fake", "80", 0, &out);
	check(status.err == wx::Err::Config && dns_calls == 0, "zero timeout rejected");
	status = net.connect(net, "fake", "80", INT_MAX, &out);
	check(status.err == wx::Err::Config && dns_calls == 0, "millisecond overflow rejected");
	reset(); poll_mode = 1;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.ok() && ticks == 20 && polls == 1 && frees == 1, "asynchronous connect succeeds");
	if (status.ok()) net.close(net, out);
	reset(); poll_mode = 1; socket_error = ECONNREFUSED;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Connect && status.detail == ECONNREFUSED && closes == 3 && frees == 1, "socket error falls back and closes every descriptor");
	reset(); poll_mode = 2; clock_errno_noise = true;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Timeout && ticks == 1000 && connects == 1 && frees == 1, "EINTR cannot restart budget");
	reset(); poll_mode = 3; clock_errno_noise = true;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Connect && status.detail == EIO && closes == 3 && frees == 1, "poll errors close descriptors");
	reset(); dns_error = EAI_NONAME;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Dns && status.detail == EAI_NONAME && connects == 0 && frees == 0, "lookup error preserved inside budget");
	reset(); ready = true;
	status = net.connect(net, "fake", "80", 1, &out);
	clock_fail_at = clock_calls + 1;
	char byte;
	check(status.ok() && out.read(out, &byte, 1) == -1 && out.err == wx::Err::Clock, "I/O wait fails closed without clock");
	if (status.ok()) net.close(net, out);
	reset(); clock_fail_at = 1;
	check(!net.sleep_ms(net, 10), "sleep fails without clock");
	reset(); flag_failure = 1;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Connect && status.detail == EIO && connects == 0 && closes == 3 && frees == 1, "cannot read descriptor flags");
	reset(); flag_failure = 2;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Connect && status.detail == EPERM && connects == 0 && closes == 3 && frees == 1, "cannot enable nonblocking mode");
	reset(); flag_failure = 3; ready = true;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Connect && status.detail == EPERM && closes == 3 && frees == 1, "cannot restore flags after success");
	reset(); poll_mode = 1; option_failure = true;
	status = net.connect(net, "fake", "80", 1, &out);
	check(status.err == wx::Err::Connect && status.detail == ENOTSOCK && closes == 3 && frees == 1, "cannot inspect pending socket error");
	const wx::TimeBudget short_budget{0, 120}; net.budget = &short_budget;
	reset();
	status = net.connect(net, "fake", "80", 10, &out);
	check(status.err == wx::Err::Timeout && ticks == 120 && connects == 1, "outer budget caps connect wait");
	reset(); ready = true; connect_delay = 60;
	status = net.connect(net, "fake", "80", 10, &out);
	check(status.ok(), "connect succeeds before outer deadline");
	if (status.ok()) {
		check(out.read(out, &byte, 1) == -1 && out.err == wx::Err::Timeout && ticks == 120, "outer budget caps silent read");
		net.close(net, out);
	}
	reset(); ready = true;
	status = net.connect(net, "fake", "80", 10, &out);
	if (status.ok()) {
		check(out.write(out, "x", 1) == -1 && out.err == wx::Err::Timeout && ticks == 120, "outer budget caps blocked write");
		net.close(net, out);
	} else check(false, "write setup");
	reset(); const wx::TimeBudget expired{0, 0}; net.budget = &expired;
	status = net.connect(net, "fake", "80", 10, &out);
	check(status.err == wx::Err::Timeout && dns_calls == 0, "expired outer budget skips lookup");
	reset(); const wx::TimeBudget backwards{1000, 100}; net.budget = &backwards;
	status = net.connect(net, "fake", "80", 10, &out);
	check(status.err == wx::Err::Clock && dns_calls == 0, "invalid outer clock skips lookup");
	net.budget = nullptr;
	printf("connect budget: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
