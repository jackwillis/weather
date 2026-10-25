// "weather" for Mac OS 9, console edition: fetch the forecast and print it. The real OS 9 window UI comes later;
// this proves the whole stack (Open Transport -> TLS -> HTTP -> JSON -> core) on the real machine.
#include "core/config.hpp"
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
	EventRecord ev;
	while (WaitNextEvent(everyEvent, &ev, 0, nullptr))
		if (ev.what == keyDown && (ev.modifiers & cmdKey) && (ev.message & charCodeMask) == '.') return false;
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
	// Optional "weather.conf" next to the app: same key = value lines as ~/.weatherrc.
	if (FILE *f = fopen("weather.conf", "r")) {
		char line[160];
		int n = 0;
		while (fgets(line, sizeof line, f)) {
			n++;
			sv l(line);
			while (!l.empty() && (l.back() == '\n' || l.back() == '\r' || l.back() == ' ')) l.remove_suffix(1);
			while (!l.empty() && l.front() == ' ') l.remove_prefix(1);
			if (l.empty() || l.front() == '#') continue;
			size_t eq = l.find('=');
			if (eq == sv::npos) continue;
			sv k = l.substr(0, eq), v = l.substr(eq + 1);
			while (!k.empty() && k.back() == ' ') k.remove_suffix(1);
			while (!v.empty() && v.front() == ' ') v.remove_prefix(1);
			wx::Status s = config_set(&cfg, k, v, n);
			if (!s.ok()) fprintf(stderr, "weather.conf line %d: %s\n", n, s.msg);
		}
		fclose(f);
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
