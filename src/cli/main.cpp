// weather: print current conditions. See weather.1 for usage.
#include "core/config.hpp"
#include "core/fetch.hpp"
#include "core/log.hpp"
#include "core/settings.hpp"
#include "platform/posix/net_posix.hpp"
#include "platform/posix/settings_posix.hpp"
#include "core/openmeteo.hpp"
#include "tls/tls.hpp"
#include "core/version.hpp"
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <unistd.h>

using namespace wx;

static const Provider *const kAll[] = { &kOpenMeteo };

static void usage(FILE *f)
{
	fputs("usage: weather [-1dhqsSV] [-c file] [-l lat,lon] [-n name] [-p providers] [-t secs]\n"
	      "               [-u metric|imperial] [-v]\n"
	      "  -1  one line of output       -c  config file (default ~/.weatherrc)\n"
	      "  -d  debug: dump traffic      -l  latitude,longitude in decimal degrees\n"
	      "  -n  place name for display   (-l without -n drops the configured name)\n"
	      "  -h  this help                -p  comma separated sources, in fallback order\n"
	      "  -q  errors only              -s  log to syslog\n"
	      "  -S  save location/name/units to the config file and exit\n"
	      "  -t  network timeout, secs    -u  units\n"
	      "  -v  more logging (repeat)    -V  print version\n"
	      "  -H host:port  (testing) plain HTTP to host:port instead of the provider\n"
	      "sources: open-meteo\n", f);
}

static volatile sig_atomic_t g_stop;
static void on_signal(int) { g_stop = 1; }
// Net::yield: lets Ctrl-C / SIGTERM cancel a fetch cleanly (sockets closed, exit 130).
static bool cli_yield(Net &) { return !g_stop; }

static int fail(const Status &s)
{
	fprintf(stderr, "weather: %s: %s", err_name(s.err), s.msg);
	if (s.detail) {
		if (s.err == Err::Config) fprintf(stderr, " (line %d)", s.detail);
		else fprintf(stderr, " (detail %d)", s.detail);
	}
	fputc('\n', stderr);
	return exit_code(s.err);
}

int main(int argc, char **argv)
{
	Config cfg;
	const char *cfgpath = nullptr;
	bool oneline = false, use_syslog = false, dump = false, save = false;
	int verbose = 0;
	bool quiet = false;
	const char *force = nullptr;
	char forcehost[256], *forceport = nullptr;

	signal(SIGPIPE, SIG_IGN);
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_signal;        // no SA_RESTART: poll() must wake so yield can see g_stop
	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);

	// Pass 1: config file (so flags can override it).
	for (int i = 1; i < argc - 1; i++)
		if (!strcmp(argv[i], "-c")) cfgpath = argv[i + 1];
	PosixStore store;
	Status store_st = posix_settings_init(&store, cfgpath, false);   // -S may create the file; existence is checked below
	if (store_st.ok()) {
		static char text[4096];
		size_t tn = 0;
		Status s = store.base.load(store.base, text, sizeof text, &tn);
		if (!s.ok()) return fail(s);
		if (tn) {
			s = config_parse(sv(text, tn), &cfg);
			if (!s.ok()) return fail(s);
		}
	} else if (cfgpath) {
		return fail(store_st);
	}   // no HOME and no -c: run with defaults

	static const struct option longopts[] = {
		{"help", no_argument, nullptr, 'h'},
		{"version", no_argument, nullptr, 'V'},
		{nullptr, 0, nullptr, 0},
	};
	int ch;
	while ((ch = getopt_long(argc, argv, "1c:dhH:l:n:p:qsSt:u:vV", longopts, nullptr)) != -1) {
		Status s;
		switch (ch) {
		case '1': oneline = true; break;
		case 'c': break;  // handled above
		case 'd': dump = true; verbose = 3; break;
		case 'h': usage(stdout); return 0;
		case 'H': force = optarg; break;
		case 'l': {
			char *comma = strchr(optarg, ',');
			if (!comma) return fail(make(Err::Usage, "-l wants lat,lon"));
			char la[32];
			size_t ln = static_cast<size_t>(comma - optarg);
			if (ln >= sizeof la) return fail(make(Err::Usage, "-l latitude too long"));
			memcpy(la, optarg, ln); la[ln] = 0;
			s = config_set(&cfg, "latitude", la, 0);
			if (s.ok()) s = config_set(&cfg, "longitude", comma + 1, 0);
			if (!s.ok()) return fail(make(Err::Usage, s.msg));
			cfg.name[0] = 0;   // the configured name described somewhere else
			break;
		}
		case 'n': s = config_set(&cfg, "name", optarg, 0); if (!s.ok()) return fail(make(Err::Usage, s.msg)); break;
		case 'p': s = config_set(&cfg, "providers", optarg, 0); if (!s.ok()) return fail(make(Err::Usage, s.msg)); break;
		case 'q': quiet = true; break;
		case 's': use_syslog = true; break;
		case 'S': save = true; break;
		case 't': s = config_set(&cfg, "timeout", optarg, 0); if (!s.ok()) return fail(make(Err::Usage, s.msg)); break;
		case 'u': s = config_set(&cfg, "units", optarg, 0); if (!s.ok()) return fail(make(Err::Usage, s.msg)); break;
		case 'v': verbose++; break;
		case 'V': printf("weather %s\n", WEATHER_VERSION); return 0;
		default: usage(stderr); return exit_code(Err::Usage);
		}
	}
	if (optind != argc) { usage(stderr); return exit_code(Err::Usage); }
	// A config file named with -c must exist, unless we are about to create it with -S.
	if (cfgpath && !save && access(cfgpath, F_OK) != 0)
		return fail(make(Err::Config, "cannot open config file", 0));

	Level lvl = cfg.level;
	if (verbose >= 3) lvl = LogDebug;
	else if (verbose == 2) lvl = LogDebug;
	else if (verbose == 1 && lvl < LogInfo) lvl = LogInfo;
	if (quiet) lvl = LogError;
	log_init(lvl, use_syslog, "weather");

	if (save) {
		if (!store_st.ok()) return fail(store_st);
		Status s = settings_save(store.base, cfg);
		if (!s.ok()) return fail(s);
		logf(LogInfo, "saved location, name and units to %s", store.path);
		return 0;
	}

	FetchOpts fo;
	fo.timeout_s = cfg.timeout_s;
	fo.attempts = cfg.attempts;
	fo.dump = dump;
	if (force) {
		char *colon = strrchr(const_cast<char *>(force), ':');
		if (!colon) return fail(make(Err::Usage, "-H wants host:port"));
		size_t hl = static_cast<size_t>(colon - force);
		if (hl >= sizeof forcehost) return fail(make(Err::Usage, "-H host too long"));
		memcpy(forcehost, force, hl); forcehost[hl] = 0;
		forceport = colon + 1;
		fo.force_host = forcehost;
		fo.force_port = forceport;
		logf(LogWarn, "plain HTTP to %s:%s, NO TLS (testing mode)", forcehost, forceport);
	}

	// Resolve provider names.
	const Provider *chosen[8];
	size_t nchosen = 0;
	{
		char list[sizeof cfg.providers];
		memcpy(list, cfg.providers, sizeof list);
		for (char *tok = strtok(list, ","); tok; tok = strtok(nullptr, ",")) {
			const Provider *p = nullptr;
			for (const Provider *c : kAll)
				if (!strcmp(c->name, tok)) p = c;
			if (!p) { fprintf(stderr, "weather: unknown source '%s'\n", tok); return exit_code(Err::Usage); }
			if (nchosen < 8) chosen[nchosen++] = p;
		}
	}

	Net net;
	posix_net_init(&net);
	net.yield = cli_yield;
	tls_net_init(&net);
	Query q;
	q.lat_e4 = cfg.lat_e4;
	q.lon_e4 = cfg.lon_e4;
	q.metric = cfg.metric;

	static char buf[64 * 1024];
	Forecast f;
	Status s = fetch_forecast(net, chosen, nchosen, q, fo, buf, sizeof buf, &f);
	if (!s.ok()) return fail(s);

	Buf<512> out;
	if (oneline) { format_oneline(out, f); out.add('\n'); }
	else format_full(out, f, cfg.name);
	fputs(out.c_str(), stdout);
	if (ferror(stdout) || fflush(stdout) != 0) return exit_code(Err::Io);
	return 0;
}
