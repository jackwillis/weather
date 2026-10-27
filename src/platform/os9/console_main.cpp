// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// "weather" for Mac OS 9, console edition: fetch the forecast and print it. The real OS 9 window UI comes later;
// this proves the whole stack (Open Transport -> TLS -> HTTP -> JSON -> core) on the real machine.
#include "core/config.hpp"
#include "app/config_load.hpp"
#include "app/preferences.hpp"
#include "settings_os9.hpp"
#include "config_os9.hpp"
#include "cache_os9.hpp"
#include "app/model.hpp"
#include "core/fetch.hpp"
#include "core/log.hpp"
#include "core/providers.hpp"
#include "core/version.hpp"
#include "net_os9.hpp"
#include "portable.hpp"
#include "tls/tls.hpp"
#include <Events.h>
#include <cstdio>
#include <cstring>

using namespace wx;

// Cancel with Cmd-. like any Mac program (and keep other applications alive while we wait).
static bool console_yield(Net &)
{
	// A bounded number of events per call, and every update event is validated (BeginUpdate/EndUpdate): an update
	// event that is never validated is delivered again and again, which would make this loop spin forever.
	EventRecord ev;
	for (int i = 0; i < 8 && WaitNextEvent(everyEvent, &ev, 0, nullptr); i++) {
		if (ev.what == updateEvt) {
			WindowPtr w = reinterpret_cast<WindowPtr>(ev.message);
			BeginUpdate(w);
			EndUpdate(w);
		} else if (ev.what == keyDown && (ev.modifiers & cmdKey) && (ev.message & charCodeMask) == '.') {
			return false;
		}
	}
	return true;
}

static void wait_for_click()
{
	puts("\n(click the mouse or press a key to close)");
	for (;;) {
		EventRecord ev;
		if (WaitNextEvent(mDownMask | keyDownMask, &ev, 30, nullptr)) return;
	}
}

int main()
{
	Config cfg;
	SettingsStore store = os9_config_store();
	Os9Settings preferences;
	wx::Status loaded = os9_settings_init(&preferences);
	if(loaded.ok()) loaded=load_preferences(preferences.base,store,cfg);
	if (!loaded.ok()) {
		fprintf(stderr, "settings line %d: %s\n", loaded.detail, loaded.msg);
		return exit_code(loaded.err);
	}
	log_init(cfg.level == LogWarn ? LogInfo : cfg.level, false, "weather");
	printf("weather %s for Mac OS 9\n\n", WEATHER_VERSION);

	const Provider *chosen[8];
	size_t nchosen = 0;
	wx::Status s = resolve_providers(cfg.providers, kProviders, kNumProviders, chosen, 8, &nchosen);
	int rc = 0;
	if (!s.ok()) {
		printf("weather: %s '%s'\n", s.msg, cfg.providers);
		rc = exit_code(s.err);
	} else {
		FetchOpts fo;
		fo.timeout_s = cfg.timeout_s;
		fo.attempts = cfg.attempts;
		fo.contact = cfg.contact;
		Net net;
		os9_net_init(&net);
		net.yield = console_yield;
		tls_net_init(&net);
		char clock[22];
		os9::decimal_signed(net.now(net), clock, sizeof clock);
		logf(LogInfo, "clock: unix %s UTC", clock);
		Query q;
		q.lat_e4 = cfg.lat_e4;
		q.lon_e4 = cfg.lon_e4;
		q.metric = cfg.metric;
		static char buf[64 * 1024];
		Forecast f;
		s = fetch_forecast(net, chosen, nchosen, q, fo, buf, sizeof buf, &f);
		bool cached=false;
		long long at=0,now=net.now(net);
		Os9Cache cache;
		if(cfg.cache) {
			wx::Status initialized=os9_cache_init(&cache);
			if(!initialized.ok()) logf(LogWarn,"%s",initialized.msg);
			else if(s.ok()) {
				wx::Status saved=write_cached_forecast(cache.base,cache_key(cfg),f,now);
				if(!saved.ok()) logf(LogWarn,"%s",saved.msg);
			} else if(s.err!=Err::Cancelled) {
				CachedForecast previous; bool found=false;
				wx::Status loaded=read_cached_forecast(cache.base,cache_key(cfg),now,&previous,&found);
				if(loaded.ok() && found) { f=previous.forecast; at=previous.fetched_at; cached=true; }
				else if(!loaded.ok()) logf(LogWarn,"%s",loaded.msg);
			}
		}
		if (s.ok() || cached) {
			Buf<512> out;
			if(cached) { out.add("STALE cached forecast (fetched "); Model::add_age(out,static_cast<long>(now-at)); out.add("): "); }
			format_full(out, f, cfg.name);
			fputs(out.c_str(), stdout);
		}
		if(!s.ok()) {
			printf("weather: %s: %s", err_name(s.err), s.msg);
			if (s.detail) printf(" (detail %d)", s.detail);
			putchar('\n');
			rc = exit_code(s.err);
		}
	}
	os9_net_shutdown();
	printf("\nexit status %d\n", rc);
	wait_for_click();
	return rc;
}
