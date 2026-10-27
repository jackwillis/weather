// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Exercises production Net::connect on OS 9 without touching the TLS seed store.
#include "platform/os9/net_os9.hpp"
#include <Events.h>
#include <cstdio>

namespace {
int calls, cancel_at, failed;
FILE *logfile;
bool yield(wx::Net &)
{
	EventRecord ev;
	for (int i = 0; i < 8 && WaitNextEvent(everyEvent, &ev, 0, nullptr); ++i) {
		if (ev.what == updateEvt) {
			WindowPtr w = reinterpret_cast<WindowPtr>(ev.message);
			BeginUpdate(w); EndUpdate(w);
		}
	}
	return ++calls != cancel_at;
}
void run(wx::Net &net, const char *host, wx::Err expected, int cancel)
{
	calls = 0; cancel_at = cancel;
	wx::Io io;
	UInt32 start = TickCount();
	wx::Status s = net.connect(net, host, "443", 10, &io);
	bool ok = s.err == expected;
	if (!ok) ++failed;
	std::fprintf(logfile, "%s host=%s err=%d expected=%d detail=%d ticks=%lu yields=%d message=%s\n",
	             ok ? "ok" : "FAIL", host, static_cast<int>(s.err), static_cast<int>(expected), s.detail,
	             static_cast<unsigned long>(static_cast<UInt32>(TickCount() - start)), calls, s.msg ? s.msg : "");
	std::fflush(logfile);
	if (s.ok()) net.close(net, io);
}
}

int main()
{
	logfile = std::fopen("MacOS9Lives:weather-dev:dns-net-probe.log", "w");
	if (!logfile) return 2;
	wx::Net net; wx::os9_net_init(&net); net.yield = yield;
	run(net, "api.open-meteo.com", wx::Err::Ok, 0);
	run(net, "weather-no-such-host.invalid.", wx::Err::Dns, 0);
	run(net, "api.open-meteo.com", wx::Err::Cancelled, 1);
	char host[100];
	std::snprintf(host, sizeof host, "weather-cancel-net-%lu.invalid.", static_cast<unsigned long>(TickCount()));
	run(net, host, wx::Err::Cancelled, 3);
	run(net, "api.open-meteo.com", wx::Err::Ok, 0);
	wx::os9_net_shutdown();
	std::fprintf(logfile, "production DNS checks finished: %d failed\n", failed);
	std::fclose(logfile);
	return failed ? 1 : 0;
}
