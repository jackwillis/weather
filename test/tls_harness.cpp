// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Harness: tls_harness HOST PORT CLOCK_UNIX_SECONDS [DEADLINE_S | --attempts N]
// HTTPS fetch/retry of https://HOST:PORT/ through the real fetch_once and the real tls.cpp (built by the script with a
// throwaway trust anchor, see WEATHER_TEST_ANCHORS), with a fixed clock. Prints one line: RESULT err=NAME detail=NAME|N
#include "core/fetch.hpp"
#include "core/log.hpp"
#include "platform/posix/net_posix.hpp"
#include "tls/tls.hpp"
#include <bearssl.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstring>

using namespace wx;

// Certificate fixtures choose UTC; deadlines use Net::monotonic_ms.
static long long g_clock;
static long long fixed_now(Net &) { return g_clock; }

static const char *x509_name(int d)
{
	switch (d) {
	case BR_ERR_X509_EXPIRED: return "EXPIRED";
	case BR_ERR_X509_BAD_SERVER_NAME: return "BAD_SERVER_NAME";
	case BR_ERR_X509_NOT_TRUSTED: return "NOT_TRUSTED";
	case BR_ERR_X509_NOT_CA: return "NOT_CA";
	case BR_ERR_X509_TIME_UNKNOWN: return "TIME_UNKNOWN";
	default: return nullptr;
	}
}

int main(int argc, char **argv)
{
	if (argc != 4 && argc != 5 && !(argc == 6 && !strcmp(argv[4],"--attempts"))) { fprintf(stderr, "usage: tls_harness HOST PORT CLOCK_UNIX_SECONDS [DEADLINE_S | --attempts 1..3]\n"); return 2; }
	alarm(40); // A stuck local fixture must fail instead of hanging the suite.
	log_init(LogError, false, "tls_harness");
	g_clock = atoll(argv[3]);
	Net net;
	posix_net_init(&net);
	tls_net_init(&net);
	net.now = fixed_now;
	FetchOpts o;
	o.timeout_s = 5;
	o.attempts = argc==6?atoi(argv[5]):1;
	if(o.attempts<1 || o.attempts>3) return 2;
	if(argc==5) { o.deadline_s=atoi(argv[4]); if(o.deadline_s<1 || o.deadline_s>30) return 2; }
	static char buf[4096];
	sv body;
	Status s = fetch_retry(net, argv[1], argv[2], true, "/", o, buf, sizeof buf, &body);
	const char *dn = s.err == Err::Timeout && s.detail == ETIMEDOUT ? "ETIMEDOUT" : x509_name(s.detail);
	if (dn) printf("RESULT err=%s detail=%s\n", err_name(s.err), dn);
	else printf("RESULT err=%s detail=%d\n", err_name(s.err), s.detail);
	return 0;
}
