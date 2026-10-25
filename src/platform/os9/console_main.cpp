// "weather" for Mac OS 9, console edition: fetch the forecast and print it. The real OS 9 window UI comes later;
// this proves the whole stack (Open Transport -> TLS -> HTTP -> JSON -> core) on the real machine.
#include "core/config.hpp"
#include "app/config_load.hpp"
#include "config_os9.hpp"
#include "core/fetch.hpp"
#include "core/log.hpp"
#include "core/providers.hpp"
#include "core/version.hpp"
#include "net_os9.hpp"
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
	wx::Status loaded = load_config(store, cfg);
	if (!loaded.ok()) {
		fprintf(stderr, "weather.conf line %d: %s\n", loaded.detail, loaded.msg);
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
		logf(LogInfo, "clock: unix %lu UTC", static_cast<unsigned long>(net.now(net)));   // Retro68 printf has no %lld
		Query q;
		q.lat_e4 = cfg.lat_e4;
		q.lon_e4 = cfg.lon_e4;
		q.metric = cfg.metric;
		static char buf[64 * 1024];
		Forecast f;
		s = fetch_forecast(net, chosen, nchosen, q, fo, buf, sizeof buf, &f);
		if (s.ok()) {
			Buf<512> out;
			format_full(out, f, cfg.name);
			fputs(out.c_str(), stdout);
		} else {
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
