// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Real TLS fetch and seed rotation check. Never logs random or seed bytes.
#include "platform/os9/net_os9.hpp"
#include "platform/os9/seed_store_os9.hpp"
#include "tls/seed_pool.hpp"
#include "tls/tls.hpp"
#include "core/fetch.hpp"
#include "core/providers.hpp"
#include <Events.h>
#include <cstdio>
#include <cstring>

namespace {
bool yield(wx::Net &)
{
	EventRecord ev;
	for (int i = 0; i < 8 && WaitNextEvent(everyEvent, &ev, 0, nullptr); ++i) {
		if (ev.what == updateEvt) {
			WindowPtr w = reinterpret_cast<WindowPtr>(ev.message);
			BeginUpdate(w); EndUpdate(w);
		}
	}
	return true;
}
bool snapshot(unsigned char *record)
{
	wx::Os9SeedStore store;
	if (!store.acquire()) return false;
	size_t n = 0; unsigned char decoded[32];
	bool ok = store.read(record, wx::kSeedRecordSize, n) && wx::seed_decode(record, n, decoded);
	wx::secret_clear(decoded, sizeof decoded);
	bool released = store.release();
	return ok && released;
}
}
int main()
{
	FILE *log = std::fopen("MacOS9Lives:weather-dev:tls-seed-probe.log", "a");
	if (!log) return 2;
	unsigned char before[wx::kSeedRecordSize], after[wx::kSeedRecordSize];
	bool have_before = snapshot(before);
	wx::Net net; wx::os9_net_init(&net); wx::tls_net_init(&net); net.yield = yield;
	wx::FetchOpts opts; opts.attempts = 1; opts.timeout_s = 15;
	wx::Query query;
	const wx::Provider *providers[] = {&wx::kOpenMeteo};
	static char buffer[64 * 1024]; wx::Forecast forecast;
	wx::Status s = wx::fetch_forecast(net, providers, 1, query, opts, buffer, sizeof buffer, &forecast);
	wx::os9_net_shutdown();
	bool have_after = snapshot(after);
	bool advanced = have_before && have_after && std::memcmp(before, after, sizeof before) != 0;
	bool ok = s.ok() && advanced;
	std::fprintf(log, "%s live verified-TLS fetch: err=%d detail=%d message=%s seed-advanced=%d\n",
	             ok ? "ok" : "FAIL", static_cast<int>(s.err), s.detail, s.msg ? s.msg : "", advanced);
	wx::secret_clear(before, sizeof before); wx::secret_clear(after, sizeof after);
	std::fclose(log);
	return ok ? 0 : 1;
}
