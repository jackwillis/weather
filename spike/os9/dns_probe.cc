// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Diagnostic only. Public API references and coverage limits: DNS-PROBE.md.
#include <Events.h>
#include <cstdio>
#include <cstring>
#include "ot_min.h"

namespace {
constexpr UInt32 kOpenComplete = 0x20000007;
constexpr UInt32 kResolveComplete = 0x10000001;
struct Operation {
	char host[256];
	InetHostInfo info;
	ProviderRef provider;
	OTResult open_result, query_result;
	void *query_cookie;
	UInt32 opened, resolved, callbacks;
};
// Never reuse these buffers, including after close. A late callback stays safe.
Operation operations[5];
FILE *logfile;
int failures;
unsigned long pumps;

UInt32 load(const UInt32 &word) { return __atomic_load_n(&word, __ATOMIC_ACQUIRE); }
void publish(UInt32 &word) { __atomic_store_n(&word, 1, __ATOMIC_RELEASE); }

pascal void notify(void *context, UInt32 event, OTResult result, void *cookie)
{
	Operation &op = *static_cast<Operation *>(context);
	__atomic_fetch_add(&op.callbacks, 1, __ATOMIC_RELAXED);
	if (event == kOpenComplete) {
		op.provider = cookie; op.open_result = result;
		publish(op.opened);
	} else if (event == kResolveComplete) {
		op.query_result = result; op.query_cookie = cookie;
		publish(op.resolved);
	}
}

void check(bool ok, const char *label)
{
	if (!ok) ++failures;
	std::fprintf(logfile, "%s %s\n", ok ? "ok" : "FAIL", label);
	std::fflush(logfile);
}

void pump()
{
	++pumps;
	EventRecord ev;
	for (int i = 0; i < 8 && WaitNextEvent(everyEvent, &ev, 0, nullptr); ++i) {
		if (ev.what == updateEvt) {
			WindowPtr w = reinterpret_cast<WindowPtr>(ev.message);
			BeginUpdate(w); EndUpdate(w);
		}
	}
	WaitNextEvent(0, &ev, 3, nullptr);
}

bool wait(const UInt32 &flag, UInt32 ticks)
{
	UInt32 start = TickCount();
	while (!load(flag) && static_cast<UInt32>(TickCount() - start) < ticks) pump();
	return load(flag) != 0;
}

bool run(int index, const char *host, bool expect_success, bool abandon)
{
	Operation &op = operations[index];
	std::snprintf(op.host, sizeof op.host, "%s", host);
	std::fprintf(logfile, "case %d: %s abandon=%d\n", index, host, abandon);
	std::fflush(logfile);
	OSStatus e = weather_async_open_internet(notify, &op);
	check(e == 0, "async open accepted");
	if (e) return false;
	if (!wait(op.opened, 900)) {
		check(false, "async open completed within 15 seconds; retaining pending context until OT shutdown");
		return false;
	}
	check(op.open_result == 0 && op.provider, "open callback delivered provider");
	if (op.open_result || !op.provider) return false;
	check(OTSetNonBlocking(op.provider) == 0, "provider nonblocking");
	UInt32 started = TickCount();
	e = OTInetStringToAddress(op.provider, op.host, &op.info);
	std::fprintf(logfile, "query initial=%ld callback-before-return=%lu\n", static_cast<long>(e),
	             static_cast<unsigned long>(load(op.resolved)));
	check(e == 0, "async query accepted");
	if (!e && !abandon) {
		bool done = wait(op.resolved, 900);
		check(done, "query completed within 15 seconds");
		if (done) {
			check(op.query_cookie == &op.info, "completion cookie identifies output buffer");
			check(expect_success ? op.query_result == 0 && op.info.addrs[0] != 0 : op.query_result != 0,
			      "resolver result matches expected outcome");
			std::fprintf(logfile, "query result=%ld address=0x%lx\n", op.query_result,
			             static_cast<unsigned long>(op.info.addrs[0]));
		}
	}
	bool pending = !e && !load(op.resolved);
	std::fprintf(logfile, "before close: pending=%d elapsed-ticks=%lu pumps=%lu\n", pending,
	             static_cast<unsigned long>(static_cast<UInt32>(TickCount() - started)), pumps);
	if (abandon && !pending) std::fputs("INCOMPLETE pending-close coverage: query completed too early\n", logfile);
	UInt32 before_close = TickCount();
	e = OTCloseProvider(op.provider);
	check(e == 0, "provider close succeeded");
	std::fprintf(logfile, "close elapsed-ticks=%lu\n",
	             static_cast<unsigned long>(static_cast<UInt32>(TickCount() - before_close)));
	UInt32 callbacks = load(op.callbacks), settle = TickCount();
	while (static_cast<UInt32>(TickCount() - settle) < 60) pump();
	check(load(op.callbacks) == callbacks, "no callback observed in one-second post-close window");
	std::fflush(logfile);
	return e == 0;
}
}

int main()
{
	logfile = std::fopen("MacOS9Lives:weather-dev:dns-probe.log", "w");
	if (!logfile) return 2;
	std::fputs("DNS probe start: permanent per-case contexts, no callback file I/O\n", logfile);
	check(InitOpenTransport() == 0, "Open Transport initialized");
	if (!failures) {
		run(0, "127.0.0.1", true, false);
		run(1, "api.open-meteo.com", true, false);
		run(2, "weather-probe-does-not-exist.invalid.", false, false);
		char name[100];
		std::snprintf(name, sizeof name, "weather-cancel-%lu.invalid.", static_cast<unsigned long>(TickCount()));
		run(3, name, false, true);
		run(4, "api.open-meteo.com", true, false);
	}
	CloseOpenTransport();
	std::fprintf(logfile, "DNS probe finished: %d failed; pumps=%lu\n", failures, pumps);
	std::fclose(logfile);
	return failures ? 1 : 0;
}
