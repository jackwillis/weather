// Open Transport backend. One endpoint at a time (a small windowed or console app).
// Endpoints are synchronous + non-blocking: every call returns at once and "no data yet" is polled, so Net::yield
// runs about every 50 ms (the event loop stays alive and the user can cancel). Known limit, like getaddrinfo on
// POSIX: the DNS lookup itself (OTInetStringToAddress) cannot be interrupted.
#include "net_os9.hpp"
#include "ot_min.h"
#include "core/log.hpp"
#include <bearssl.h>
#include <Events.h>
#include <OSUtils.h>
#include "toolbox_extra.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace wx {

namespace {

struct Conn {
	EndpointRef ep = nullptr;
	int timeout_ms = 15000;
	Net *net = nullptr;
};
Conn g_conn;
bool g_ot_ok = false;

constexpr long kSliceTicks = 3;   // 3/60 s = 50 ms

long long mono_ms() { return static_cast<long long>(TickCount()) * 1000 / 60; }

enum Wait { WaitOk = 0, WaitTimeout, WaitCancelled };

// One polling pause: let the app run (and maybe cancel), sleep one slice, enforce the deadline.
Wait pause(Net *net, long long deadline_ms)
{
	static long polls = 0;
	if (++polls % 200 == 0) logf(LogDebug, "still waiting (%ld polls, %lld ms to deadline)", polls, deadline_ms - mono_ms());
	if (net && net->yield && !net->yield(*net)) return WaitCancelled;
	long fin;
	Delay(kSliceTicks, &fin);
	return mono_ms() >= deadline_ms ? WaitTimeout : WaitOk;
}

void fail_wait(Io &io, Wait w)
{
	if (w == WaitTimeout) { io.err = Err::Timeout; io.detail = 0; io.msg = "timed out waiting for the server"; }
	else { io.err = Err::Cancelled; io.detail = 0; io.msg = "cancelled"; }
}

void fail_io(Io &io, long code, const char *msg)
{
	io.err = Err::Io; io.detail = static_cast<int>(code); io.msg = msg;
}

// OTLook told us something happened. Returns 1 = peer finished normally (EOF), 0 = harmless, retry, -1 = connection broken.
int handle_look(EndpointRef ep)
{
	long ev = OTLook(ep);
	logf(LogDebug, "OTLook -> 0x%lx", ev);
	if (ev == kT_ORDREL) { OTRcvOrderlyDisconnect(ep); return 1; }
	if (ev == kT_DISCONNECT) { TDiscon d; std::memset(&d, 0, sizeof d); OTRcvDisconnect(ep, &d); return -1; }
	return 0;   // T_DATA, T_GODATA, ...: just retry the call
}

long ot_read(Io &io, void *buf, size_t len)
{
	Conn *c = static_cast<Conn *>(io.ctx);
	long long deadline = mono_ms() + c->timeout_ms;
	for (;;) {
		OTFlags fl = 0;
		long n = OTRcv(c->ep, buf, len, &fl);
		if (n > 0) return n;
		if (n == kOTNoDataErr || n == 0) {
			Wait w = pause(c->net, deadline);
			if (w != WaitOk) { fail_wait(io, w); return -1; }
			continue;
		}
		if (n == kOTLookErr) {
			int h = handle_look(c->ep);
			if (h > 0) return 0;
			if (h < 0) { fail_io(io, n, "connection reset by the server"); return -1; }
			Wait w = pause(c->net, deadline);   // never spin: yield and honour the deadline
			if (w != WaitOk) { fail_wait(io, w); return -1; }
			continue;
		}
		logf(LogDebug, "OTRcv -> %ld", n);
		fail_io(io, n, "network I/O error");
		return -1;
	}
}

long ot_write(Io &io, const void *buf, size_t len)
{
	Conn *c = static_cast<Conn *>(io.ctx);
	const char *p = static_cast<const char *>(buf);
	size_t done = 0;
	long long deadline = mono_ms() + c->timeout_ms;
	while (done < len) {
		long n = OTSnd(c->ep, const_cast<char *>(p + done), len - done, 0);
		if (n > 0) { done += static_cast<size_t>(n); deadline = mono_ms() + c->timeout_ms; continue; }
		if (n == kOTFlowErr || n == kOTNoDataErr || n == 0) {
			Wait w = pause(c->net, deadline);
			if (w != WaitOk) { fail_wait(io, w); return -1; }
			continue;
		}
		if (n == kOTLookErr) {
			int h = handle_look(c->ep);
			if (h != 0) { fail_io(io, n, "connection closed by the server"); return -1; }
			Wait w = pause(c->net, deadline);
			if (w != WaitOk) { fail_wait(io, w); return -1; }
			continue;
		}
		fail_io(io, n, "network I/O error");
		return -1;
	}
	return static_cast<long>(done);
}

Status connect_error(long code, const char *what)
{
	if (code == kOTETIMEDOUTErr) return make(Err::Timeout, "connect timed out", static_cast<int>(code));
	return make(Err::Connect, what, static_cast<int>(code));
}

// Connect one endpoint to ip:port. On success stores it in g_conn.
Status connect_ip(Net &net, UInt32 ip, UInt16 port, int timeout_ms)
{
	TEndpointInfo info;
	OSStatus err = 0;
	EndpointRef ep = OTOpenEndpoint(OTCreateConfiguration("tcp"), 0, &info, &err);
	if (!ep || err) return make(Err::Connect, "cannot open a TCP endpoint", err);
	OSStatus m1 = OTSetSynchronous(ep), m2 = OTSetNonBlocking(ep);
	logf(LogDebug, "OTSetSynchronous -> %ld, OTSetNonBlocking -> %ld", static_cast<long>(m1), static_cast<long>(m2));
	OSStatus e = OTBind(ep, nullptr, nullptr);
	if (e) { OTCloseProvider(ep); return make(Err::Connect, "cannot bind a TCP endpoint", e); }

	InetAddress a;
	std::memset(&a, 0, sizeof a);
	a.fAddressType = 2;   // AF_INET
	a.fPort = port;       // big-endian machine: host order == network order
	a.fHost = ip;
	TCall call;
	std::memset(&call, 0, sizeof call);
	call.addr.len = call.addr.maxlen = sizeof a;
	call.addr.buf = reinterpret_cast<UInt8 *>(&a);

	long long deadline = mono_ms() + timeout_ms;
	e = OTConnect(ep, &call, nullptr);
	while (e == kOTNoDataErr || e == kOTLookErr) {
		if (e == kOTLookErr) {
			long ev = OTLook(ep);
			if (ev == kT_DISCONNECT) {
				TDiscon d; std::memset(&d, 0, sizeof d);
				OTRcvDisconnect(ep, &d);
				OTCloseProvider(ep);
				return connect_error(d.reason ? d.reason : kOTECONNREFUSEDErr, "connection refused or unreachable");
			}
		}
		Wait w = pause(&net, deadline);
		if (w != WaitOk) {
			OTCloseProvider(ep);
			return w == WaitCancelled ? make(Err::Cancelled, "cancelled while connecting") : make(Err::Timeout, "connect timed out");
		}
		e = OTRcvConnect(ep, nullptr);
	}
	if (e) { OTCloseProvider(ep); return connect_error(e, "cannot connect"); }
	g_conn.ep = ep;
	logf(LogDebug, "tcp connected");
	return okay();
}

Status os9_connect(Net &net, const char *host, const char *port, int timeout_s, Io *out)
{
	if (!g_ot_ok) {
		OSStatus e = InitOpenTransport();
		if (e) return make(Err::Connect, "Open Transport is not available (is TCP/IP set up?)", e);
		g_ot_ok = true;
	}
	char *endp = nullptr;
	long pnum = std::strtol(port, &endp, 10);
	if (*port == 0 || *endp != 0 || pnum < 1 || pnum > 65535) return make(Err::Usage, "bad port number");

	OSStatus err = 0;
	InetSvcRef svc = OTOpenInternetServices(kOTDefaultInternetServicesPath, 0, &err);
	if (!svc || err) return make(Err::Dns, "cannot open the Open Transport name service", err);
	InetHostInfo hi;
	std::memset(&hi, 0, sizeof hi);
	OSStatus e = OTInetStringToAddress(svc, const_cast<char *>(host), &hi);
	OTCloseProvider(svc);
	if (e || hi.addrs[0] == 0) {
		logf(LogDebug, "OTInetStringToAddress(%s) -> %ld", host, static_cast<long>(e));
		return make(Err::Dns, "cannot resolve host name", e);
	}

	Status last = make(Err::Connect, "cannot connect");
	for (int i = 0; i < 10 && hi.addrs[i]; i++) {
		last = connect_ip(net, hi.addrs[i], static_cast<UInt16>(pnum), timeout_s * 1000);
		if (last.ok() || last.err == Err::Cancelled) break;
	}
	if (!last.ok()) return last;
	g_conn.timeout_ms = timeout_s * 1000;
	g_conn.net = &net;
	out->ctx = &g_conn;
	out->read = ot_read;
	out->write = ot_write;
	out->err = Err::Ok;
	out->detail = 0;
	out->msg = nullptr;
	return okay();
}

void os9_close(Net &, Io &io)
{
	Conn *c = static_cast<Conn *>(io.ctx);
	if (!c || !c->ep) return;
	OTSndOrderlyDisconnect(c->ep);
	OTCloseProvider(c->ep);
	c->ep = nullptr;
}

bool os9_sleep(Net &net, long ms)
{
	long long end = mono_ms() + ms;
	while (mono_ms() < end)
		if (pause(&net, end) == WaitCancelled) return false;
	return true;
}

// Unix seconds (UTC). The Mac clock is local wall-clock seconds since 1904; the Date & Time control panel's
// time zone gives the offset: gmtDelta (low 24 bits, signed seconds east of GMT, daylight saving included).
long long os9_now(Net &)
{
	UInt32 mac;
	GetDateTime(&mac);
	MachineLocation loc;
	ReadLocation(&loc);
	long delta = loc.u.gmtDelta & 0x00FFFFFF;
	if (delta & 0x00800000) delta |= ~0x00FFFFFFL;
	bool dst = (loc.u.gmtDelta & 0x80000000UL) != 0;
	long long local = static_cast<long long>(mac) - 2082844800LL;   // 1904 -> 1970
	static bool shown = false;
	if (!shown) { shown = true; logf(LogDebug, "clock: mac=%lu gmtDelta=%ld dstflag=%d raw=0x%lx", static_cast<unsigned long>(mac), delta, dst ? 1 : 0, static_cast<unsigned long>(loc.u.gmtDelta)); }
	// Verified against OS X on the real machine (2026-10-25, CDT): gmtDelta is -18000 and ALREADY includes daylight saving,
	// the flag in the high byte is informational. Subtracting another hour for the flag made the clock an hour slow.
	return local - delta;
}

// Entropy. Mac OS 9 has no /dev/urandom, so we hash many weak sources: timer jitter over a busy loop, ticks, the clock,
// the mouse, free memory, plus a seed file kept from the previous run. This is NOT as strong as a kernel RNG; it is
// good enough to make session keys unguessable by a casual observer but a determined attacker who knows the machine's
// state could narrow it. Server authentication (the certificate check) does not depend on it. See docs/ARCHITECTURE.md.
constexpr const char *kSeedFile = "weather.seed";

bool os9_entropy(Net &, void *buf, size_t len)
{
	br_sha256_context h;
	br_sha256_init(&h);
	for (int round = 0; round < 256; round++) {
		UnsignedWide us;
		Microseconds(&us);
		volatile unsigned long spin = 0;
		for (int i = 0; i < 50 + (static_cast<int>(us.lo) & 63); i++) spin += static_cast<unsigned long>(i) * us.lo;
		UnsignedWide us2;
		Microseconds(&us2);
		unsigned long sample[4] = { us.lo, us2.lo - us.lo, TickCount(), spin };
		br_sha256_update(&h, sample, sizeof sample);
	}
	UInt32 now;
	GetDateTime(&now);
	Point mouse;
	GetMouse(&mouse);
	long mem[3] = { static_cast<long>(now), static_cast<long>(FreeMem()), (static_cast<long>(mouse.h) << 16) ^ mouse.v };
	br_sha256_update(&h, mem, sizeof mem);
	unsigned char old[32];
	size_t have = 0;
	if (FILE *f = std::fopen(kSeedFile, "rb")) { have = std::fread(old, 1, sizeof old, f); std::fclose(f); }
	br_sha256_update(&h, old, have);
	unsigned char key[32];
	br_sha256_out(&h, key);

	// expand: out = SHA256(key || counter); next seed = SHA256(key || "next")
	unsigned char *p = static_cast<unsigned char *>(buf);
	for (unsigned char ctr = 0; len > 0; ctr++) {
		br_sha256_context e;
		br_sha256_init(&e);
		br_sha256_update(&e, key, sizeof key);
		br_sha256_update(&e, &ctr, 1);
		unsigned char blk[32];
		br_sha256_out(&e, blk);
		size_t n = len < sizeof blk ? len : sizeof blk;
		std::memcpy(p, blk, n);
		p += n; len -= n;
	}
	br_sha256_context nx;
	br_sha256_init(&nx);
	br_sha256_update(&nx, key, sizeof key);
	br_sha256_update(&nx, "next", 4);
	unsigned char nextseed[32];
	br_sha256_out(&nx, nextseed);
	if (FILE *f = std::fopen(kSeedFile, "wb")) { std::fwrite(nextseed, 1, sizeof nextseed, f); std::fclose(f); }
	logf(LogDebug, "entropy: jitter pool%s", have ? " + seed file" : " (no seed file yet)");
	return true;
}

} // namespace

void os9_net_shutdown()
{
	if (g_conn.ep) { OTCloseProvider(g_conn.ep); g_conn.ep = nullptr; }
	if (g_ot_ok) { CloseOpenTransport(); g_ot_ok = false; }
}

void os9_net_init(Net *net)
{
	net->connect = os9_connect;
	net->close = os9_close;
	net->sleep_ms = os9_sleep;
	net->now = os9_now;
	net->entropy = os9_entropy;
}

} // namespace wx
