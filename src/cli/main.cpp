// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// belike: print current conditions. See belike.1 for usage.
#include "core/config.hpp"
#include "core/geocoding.hpp"
#include "app/model.hpp"
#include "platform/posix/cache_posix.hpp"
#include "core/fetch.hpp"
#include "core/log.hpp"
#include "core/settings.hpp"
#include "platform/posix/net_posix.hpp"
#include "platform/posix/settings_posix.hpp"
#include "core/providers.hpp"
#include "tls/tls.hpp"
#include "core/version.hpp"
#include "app/credits.hpp"
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <unistd.h>

using namespace wx;

static void usage(FILE *f)
{
	fputs("usage: belike [-1CdhqsSV] [-c file] [-l lat,lon] [-n name] [-p providers] [-t secs]\n"
	      "               [-u metric|imperial] [-v] [-g query]\n"
              "  -g  search places and exit; does not save or fetch weather\n"
	      "  -1  one line of output       -c  config file (default ~/.belikerc)\n"
	      "  -d  debug: dump traffic      -l  latitude,longitude in decimal degrees\n"
	      "  -n  place name for display   (-l without -n drops the configured name)\n"
	      "  -h  this help                -p  comma separated sources, in fallback order\n"
	      "  -q  errors only              -s  log to syslog\n"
	      "  -S  save location/name/units to the config file and exit\n"
	      "  -t  network timeout, secs    -u  units\n"
	      "  -v  more logging (repeat)    -V  print version and licence\n"
	      "  -C  (--credits) licence, data and software credits\n"
	      "  -H host:port  (testing) plain HTTP to host:port instead of the provider\n"
	      "sources: open-meteo, met-no, nws (met-no needs 'contact' in the config file)\n", f);
}

static volatile sig_atomic_t g_stop;
static void on_signal(int) { g_stop = 1; }
// Net::yield: lets Ctrl-C / SIGTERM cancel a fetch cleanly (sockets closed, exit 130).
static bool cli_yield(Net &) { return !g_stop; }

static int fail(const Status &s)
{
	fprintf(stderr, "belike: %s: %s", err_name(s.err), s.msg);
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
	const char *place_query = nullptr;
	char forcehost[256], *forceport = nullptr;

	signal(SIGPIPE, SIG_IGN);
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_signal;        // no SA_RESTART: poll() must wake so yield can see g_stop
	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);

	// Options that edit the config are recorded, then applied after the config file is loaded, so a flag overrides the file
	// whichever way -c is written (-c FILE, -cFILE, -Sc FILE) and wherever it appears.
	struct Pending { char ch; const char *arg; } pending[32];
	size_t npending = 0;

	static const struct option longopts[] = {
		{"help", no_argument, nullptr, 'h'},
		{"search", required_argument, nullptr, 'g'},
		{"version", no_argument, nullptr, 'V'},
		{"credits", no_argument, nullptr, 'C'},
		{nullptr, 0, nullptr, 0},
	};
	int ch;
	while ((ch = getopt_long(argc, argv, "1c:Cdg:hH:l:n:p:qsSt:u:vV", longopts, nullptr)) != -1) {
		switch (ch) {
		case '1': oneline = true; break;
		case 'c': cfgpath = optarg; break;
		case 'd': dump = true; verbose = 3; break;
		case 'h': usage(stdout); return 0;
		case 'g':
			if(place_query) return fail(make(Err::Usage,"give one search query per invocation"));
			place_query=optarg; break;
		case 'H': force = optarg; break;
		case 'l': case 'n': case 'p': case 't': case 'u':
			if (npending == sizeof pending / sizeof *pending) return fail(make(Err::Usage, "too many options"));
			pending[npending].ch = static_cast<char>(ch);
			pending[npending++].arg = optarg;
			break;
		case 'q': quiet = true; break;
		case 's': use_syslog = true; break;
		case 'S': save = true; break;
		case 'v': verbose++; break;
		case 'V':
			printf("belike %s\n", WEATHER_VERSION);
			for (const char *line : kCreditsShort) printf("%s\n", line);
			puts("Run 'belike --credits' for all credits.");
			return 0;
		case 'C':
			for (size_t i = 0; i < kCreditPageCount; i++) {
				printf("%s%s\n", i ? "\n" : "", kCreditPages[i].title);
				for (size_t k = 0; k < kCreditPages[i].n; k++) printf("  %s\n", kCreditPages[i].lines[k]);
			}
			return 0;
		default: usage(stderr); return exit_code(Err::Usage);
		}
	}
	if (optind != argc) { usage(stderr); return exit_code(Err::Usage); }

	if(place_query) {
        if(save || force || oneline) return fail(make(Err::Usage,"search cannot combine with -S, -H or -1"));
        for(size_t i=0;i<npending;i++) if(pending[i].ch!='t') return fail(make(Err::Usage,"search cannot combine with location, units or provider flags"));
        char path[256]; Status s=geocoding_path(place_query,path,sizeof path);
        if(!s.ok()) return fail(s);
    }

	// Config file first, then the recorded options on top of it, in the order given.
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

	for (size_t i = 0; i < npending; i++) {
		const char *arg = pending[i].arg;
		Status s;
		switch (pending[i].ch) {
		case 'l': {
			const char *comma = strchr(arg, ',');
			if (!comma) return fail(make(Err::Usage, "-l wants lat,lon"));
			char la[32];
			size_t ln = static_cast<size_t>(comma - arg);
			if (ln >= sizeof la) return fail(make(Err::Usage, "-l latitude too long"));
			memcpy(la, arg, ln); la[ln] = 0;
			s = config_set(&cfg, "latitude", la, 0);
			if (s.ok()) s = config_set(&cfg, "longitude", comma + 1, 0);
			if (s.ok()) cfg.name[0] = 0;   // the configured name described somewhere else
			break;
		}
		case 'n': s = config_set(&cfg, "name", arg, 0); break;
		case 'p': s = config_set(&cfg, "providers", arg, 0); break;
		case 't': s = config_set(&cfg, "timeout", arg, 0); break;
		case 'u': s = config_set(&cfg, "units", arg, 0); break;
		}
		if (!s.ok()) return fail(make(Err::Usage, s.msg));
	}
	// A config file named with -c must exist, unless we are about to create it with -S.
	if (cfgpath && !save && access(cfgpath, F_OK) != 0)
		return fail(make(Err::Config, "cannot open config file", 0));

	Level lvl = cfg.level;
	if (verbose >= 3) lvl = LogDebug;
	else if (verbose == 2) lvl = LogDebug;
	else if (verbose == 1 && lvl < LogInfo) lvl = LogInfo;
	if (quiet) lvl = LogError;
	log_init(lvl, use_syslog, "belike");

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
	fo.contact = cfg.contact;
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

	if(place_query) {
        Net search_net; posix_net_init(&search_net); search_net.yield=cli_yield; tls_net_init(&search_net);
        static char search_buffer[40*1024]; PlaceResults places;
        // A place lookup is separately bounded even if forecast retries are higher.
        if(fo.attempts>3) fo.attempts=3;
        if(fo.timeout_s>60) fo.timeout_s=60;
        Status s=search_places(search_net,place_query,fo,search_buffer,sizeof search_buffer,&places);
        if(!s.ok()) return fail(s);
        Buf<2048> output; format_places(output,places);
        fputs(output.c_str(),stdout);
        if(ferror(stdout) || fflush(stdout)!=0) return exit_code(Err::Io);
        return 0;
    }

	// Resolve provider names.
	const Provider *chosen[8];
	size_t nchosen = 0;
	{
		Status s = resolve_providers(cfg.providers, kProviders, kNumProviders, chosen, 8, &nchosen);
		if (!s.ok()) { fprintf(stderr, "belike: %s '%s'\n", s.msg, cfg.providers); return exit_code(s.err); }
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
	PosixCache cache;
	bool cache_enabled=false, cached=false;
	long long fetched_at=0, now=net.now(net);
	if(!fo.force_host && store_st.ok()) {
		Buf<1024> path; path.add(store.path).add(".cache");
		Status initialized=path.truncated?make(Err::Config,"cache path too long"):posix_cache_init(&cache,path.c_str());
		cache_enabled=initialized.ok();
		if(!initialized.ok()) logf(LogWarn,"%s",initialized.msg);
	}
	if(cache_enabled && !cfg.cache) {
        Status removed=remove_cached_forecast(cache.base);
        if(!removed.ok()) logf(LogWarn,"%s",removed.msg);
    }
    fo.save_forecasts=cfg.cache && (cache_enabled || fo.force_host);
    Status s=fetch_forecast_cached(net,chosen,nchosen,q,fo,buf,sizeof buf,&f,cache_enabled && cfg.cache?&cache.base:nullptr,cfg.providers);
	now=net.now(net);
	if(s.ok() && cache_enabled) {
		Status saved=update_forecast_cache(cache.base,cache_key(cfg),f,now,cfg.cache);
		if(!saved.ok()) logf(LogWarn,"%s",saved.msg);
	} else if(!s.ok() && s.err!=Err::Cancelled && cache_enabled && cfg.cache) {
		CachedForecast previous; bool found=false;
		Status loaded=read_cached_forecast(cache.base,cache_key(cfg),now,&previous,&found);
		if(loaded.ok() && found) { f=previous.forecast; fetched_at=previous.fetched_at; cached=true; }
		else if(!loaded.ok()) logf(LogWarn,"%s",loaded.msg);
	}
	if (!s.ok() && !cached) return fail(s);

	Buf<512> out;
	if(cached) {
		out.add("STALE cached forecast (fetched ");
		Model::add_age(out,static_cast<long>(now-fetched_at)); out.add("): ");
	}
	if (oneline) { format_oneline(out, f); out.add('\n'); }
	else format_full(out, f, cfg.name);
	fputs(out.c_str(), stdout);
	if (ferror(stdout) || fflush(stdout) != 0) return exit_code(Err::Io);
	return cached?fail(s):0; // Scripts retain the live failure status.
}
