// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Open Transport backend. One endpoint at a time (a small windowed or console app).
// Endpoints are synchronous + non-blocking: every call returns at once and "no data yet" is polled, so Net::yield
// runs about every 50 ms (the event loop stays alive and the user can cancel). Known limit, like getaddrinfo on
// DNS uses an asynchronous provider and the same foreground yield/deadline policy.
#include "net_os9.hpp"
#include "portable.hpp"
#include "modal_clock.hpp"
#include "ot_min.h"
#include "dns_resolve.hpp"
#include "seed_store_os9.hpp"
#include "tls/seed_pool.hpp"
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
void close_endpoint()
{
	if (g_conn.ep) { OTCloseProvider(g_conn.ep); g_conn.ep = nullptr; }
}
bool g_ot_ok = false;
os9::ModalClock g_active_clock;

// Callback-owned storage has process lifetime. The controller only resets it
// after completion/close; an abandoned open remains alive until drained.
struct DnsBackend {
	Net *net = nullptr;
	char host[256] = {};
	InetHostInfo info = {};
	ProviderRef provider = nullptr;
	OTResult open_code = 0, query_code = 0;
	void *query_cookie = nullptr;
	UInt32 open_done = 0, query_done = 0;
	static constexpr UInt32 kOpenComplete = 0x20000007;
	static constexpr UInt32 kQueryComplete = 0x10000001;
	static pascal void notify(void *context, UInt32 event, OTResult result, void *cookie)
	{
		DnsBackend &b = *static_cast<DnsBackend *>(context);
		if (event == kOpenComplete) {
			b.provider = cookie; b.open_code = result;
			__atomic_store_n(&b.open_done, 1, __ATOMIC_RELEASE);
		} else if (event == kQueryComplete) {
			b.query_code = result; b.query_cookie = cookie;
			__atomic_store_n(&b.query_done, 1, __ATOMIC_RELEASE);
		}
	}
	uint32_t now() { return g_active_clock.ticks(TickCount()); }
	bool yield() { return !net || !net->yield || net->yield(*net); }
	void sleep() { long fin; Delay(3, &fin); }
	int open(const char *name)
	{
		// No previous callback can own these fields when the controller calls open.
		std::strcpy(host, name);
		std::memset(&info, 0, sizeof info);
		provider = nullptr; open_code = query_code = 0; query_cookie = nullptr;
		__atomic_store_n(&open_done, 0, __ATOMIC_RELEASE);
		__atomic_store_n(&query_done, 0, __ATOMIC_RELEASE);
		return weather_async_open_internet(notify, this);
	}
	bool opened() { return __atomic_load_n(&open_done, __ATOMIC_ACQUIRE) != 0; }
	int open_result() { return open_code ? open_code : provider ? 0 : -1; }
	int query()
	{
		OSStatus e = OTSetNonBlocking(provider);
		return e ? e : OTInetStringToAddress(provider, host, &info);
	}
	bool resolved() { return __atomic_load_n(&query_done, __ATOMIC_ACQUIRE) != 0; }
	int query_result() { return query_cookie == &info ? query_code : -1; }
	bool has_address() { return info.addrs[0] != 0; }
	int close()
	{
		if (!provider) return 0;
		OSStatus e = OTSetNonBlocking(provider);
		if (!e) e = OTCloseProvider(provider);
		if (!e) provider = nullptr;
		return e;
	}
};
DnsBackend g_dns;
DnsResolver<DnsBackend> g_resolver(g_dns);

constexpr long kSliceTicks = 3;   // 3/60 s = 50 ms

long long mono_ms() { return g_active_clock.milliseconds(TickCount()); }
long long os9_monotonic(Net &) { return mono_ms(); }

enum Wait { WaitOk = 0, WaitTimeout, WaitCancelled };

// One polling pause: let the app run (and maybe cancel), sleep one slice, enforce the deadline.
Wait pause(Net *net, long long deadline_ms)
{
	static long polls = 0;
	if (++polls % 200 == 0) {
		char remaining[22];
		os9::decimal_signed(deadline_ms - mono_ms(), remaining, sizeof remaining);
		logf(LogDebug, "still waiting (%ld polls, %s ms to deadline)", polls, remaining);
	}
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

	if (!host || std::strlen(host) >= sizeof g_dns.host) return make(Err::Dns, "host name too long");
	if (timeout_s <= 0 || timeout_s > 86400) return make(Err::Usage, "bad DNS timeout");
	g_dns.net = &net;
	Status dns = g_resolver.resolve(host, static_cast<uint32_t>(timeout_s) * 60, &net);
	if (!dns.ok()) return dns;
	Status cleanup = g_resolver.cleanup_status();
	if (!cleanup.ok())
		logf(LogWarn, "%s (detail %d); using completed answer, later lookups require restart", cleanup.msg, cleanup.detail);
	InetHostInfo hi = g_dns.info;

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
	long delta = os9::gmt_offset(static_cast<uint32_t>(loc.u.gmtDelta));
	bool dst = (loc.u.gmtDelta & 0x80000000UL) != 0;
	static bool shown = false;
	if (!shown) { shown = true; logf(LogDebug, "clock: mac=%lu gmtDelta=%ld dstflag=%d raw=0x%lx", static_cast<unsigned long>(mac), delta, dst ? 1 : 0, static_cast<unsigned long>(loc.u.gmtDelta)); }
	// Verified against OS X on the real machine (2026-10-25, CDT): gmtDelta is -18000 and ALREADY includes daylight saving,
	// the flag in the high byte is informational. Subtracting another hour for the flag made the clock an hour slow.
	return os9::mac_to_unix(mac, static_cast<uint32_t>(loc.u.gmtDelta));
}

// The installation seed is provisioned from a modern OS CSPRNG. Timing samples
// are supplemental only; they do not establish entropy or recovery guarantees.
Os9SeedStore g_seed_store;
SeedPool<Os9SeedStore> g_seed_pool(g_seed_store);
bool g_exit_ready = false;

bool os9_entropy(Net &, void *buf, size_t len)
{
	if (!g_exit_ready) { logf(LogError, "cannot register TLS seed cleanup"); return false; }
	br_sha256_context h;
	br_sha256_init(&h);
	for (int round = 0; round < 32; ++round) {
		UnsignedWide before, after;
		Microseconds(&before);
		volatile UInt32 spin = 0;
		for (int i = 0; i < 50 + static_cast<int>(before.lo & 63); ++i)
			spin += static_cast<UInt32>(i) * before.lo;
		Microseconds(&after);
		UInt32 sample[4] = {before.lo, after.lo - before.lo, TickCount(), spin};
		br_sha256_update(&h, sample, sizeof sample);
	}
	Point mouse; GetMouse(&mouse);
	UInt32 sample[2] = {static_cast<UInt32>(FreeMem()),
	                   (static_cast<UInt32>(static_cast<UInt16>(mouse.h)) << 16) |
	                   static_cast<UInt16>(mouse.v)};
	br_sha256_update(&h, sample, sizeof sample);
	unsigned char extra[32]; br_sha256_out(&h, extra);
	secret_clear(&h, sizeof h);
	bool ok = g_seed_pool.fill(buf, len, extra, sizeof extra);
	secret_clear(extra, sizeof extra);
	if (!ok) logf(LogError, "%s", g_seed_pool.error());
	return ok;
}

} // namespace

void os9_credit_modal_ticks(uint32_t begin, uint32_t end) { g_active_clock.credit(begin, end); }
uint32_t os9_active_ticks() { return g_active_clock.ticks(TickCount()); }
long long os9_active_milliseconds() { return mono_ms(); }

const char *os9_entropy_error() { return g_exit_ready ? g_seed_pool.error() : "cannot register TLS seed cleanup"; }

void os9_net_shutdown()
{
	if (!g_seed_pool.shutdown()) logf(LogWarn, "%s", g_seed_pool.error());
	close_endpoint();
	if (g_ot_ok) { CloseOpenTransport(); g_ot_ok = false; }
}

void os9_net_init(Net *net)
{
	weather_ot_before_close(close_endpoint);
	static bool registered = false;
	if (!registered) { g_exit_ready = std::atexit(os9_net_shutdown) == 0; registered = true; }
	net->connect = os9_connect;
	net->close = os9_close;
	net->sleep_ms = os9_sleep;
	net->now = os9_now;
	net->monotonic_ms = os9_monotonic;
	net->entropy = os9_entropy;
}

} // namespace wx
