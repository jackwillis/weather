// weather: print current conditions. See weather.1 for usage.
#include "core/config.hpp"
#include "core/fetch.hpp"
#include "core/log.hpp"
#include "platform/posix/net_posix.hpp"
#include "core/openmeteo.hpp"
#include "tls/tls.hpp"
#include "core/version.hpp"
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>

using namespace wx;

static const Provider *const kAll[] = { &kOpenMeteo };

static void usage(FILE *f)
{
	fputs("usage: weather [-1dhqsV] [-c file] [-l lat,lon] [-n name] [-p providers] [-t secs]\n"
	      "               [-u metric|imperial] [-v]\n"
	      "  -1  one line of output       -c  config file (default ~/.weatherrc)\n"
	      "  -d  debug: dump traffic      -l  latitude,longitude in decimal degrees\n"
	      "  -n  place name for display   (-l without -n drops the configured name)\n"
	      "  -h  this help                -p  comma separated sources, in fallback order\n"
	      "  -q  errors only              -s  log to syslog\n"
	      "  -t  network timeout, secs    -u  units\n"
	      "  -v  more logging (repeat)    -V  print version\n"
	      "  -H host:port  (testing) plain HTTP to host:port instead of the provider\n"
	      "sources: open-meteo\n", f);
}

static Status load_file(const char *path, char *buf, size_t cap, size_t *n)
{
	FILE *f = fopen(path, "r");
	if (!f) return make(errno == ENOENT ? Err::Config : Err::Io, "cannot open config file", errno);
	*n = fread(buf, 1, cap - 1, f);
	bool big = !feof(f);
	fclose(f);
	if (big) return make(Err::Config, "config file too large", 0);
	buf[*n] = 0;
	return okay();
}

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
	char defpath[512];
	bool oneline = false, use_syslog = false, dump = false;
	int verbose = 0;
	bool quiet = false;
	const char *force = nullptr;
	char forcehost[256], *forceport = nullptr;

	signal(SIGPIPE, SIG_IGN);

	// Pass 1: config file (so flags can override it).
	for (int i = 1; i < argc - 1; i++)
		if (!strcmp(argv[i], "-c")) cfgpath = argv[i + 1];
	char text[4096];
	size_t tn = 0;
	if (cfgpath) {
		Status s = load_file(cfgpath, text, sizeof text, &tn);
		if (!s.ok()) return fail(s);
	} else if (const char *home = getenv("HOME")) {
		snprintf(defpath, sizeof defpath, "%s/.weatherrc", home);
		Status s = load_file(defpath, text, sizeof text, &tn);
		if (!s.ok() && errno != ENOENT && s.err != Err::Config) return fail(s);
		if (!s.ok() && s.detail != ENOENT) return fail(s);
		if (!s.ok()) tn = 0;
	}
	if (tn) {
		Status s = config_parse(sv(text, tn), &cfg);
		if (!s.ok()) return fail(s);
	}

	static const struct option longopts[] = {
		{"help", no_argument, nullptr, 'h'},
		{"version", no_argument, nullptr, 'V'},
		{nullptr, 0, nullptr, 0},
	};
	int ch;
	while ((ch = getopt_long(argc, argv, "1c:dhH:l:n:p:qst:u:vV", longopts, nullptr)) != -1) {
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
		case 't': s = config_set(&cfg, "timeout", optarg, 0); if (!s.ok()) return fail(make(Err::Usage, s.msg)); break;
		case 'u': s = config_set(&cfg, "units", optarg, 0); if (!s.ok()) return fail(make(Err::Usage, s.msg)); break;
		case 'v': verbose++; break;
		case 'V': printf("weather %s\n", WEATHER_VERSION); return 0;
		default: usage(stderr); return exit_code(Err::Usage);
		}
	}
	if (optind != argc) { usage(stderr); return exit_code(Err::Usage); }

	Level lvl = cfg.level;
	if (verbose >= 3) lvl = LogDebug;
	else if (verbose == 2) lvl = LogDebug;
	else if (verbose == 1 && lvl < LogInfo) lvl = LogInfo;
	if (quiet) lvl = LogError;
	log_init(lvl, use_syslog, "weather");

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
