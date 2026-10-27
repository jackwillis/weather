// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Harness for test/tls_local.sh: tls_harness HOST PORT CLOCK_UNIX_SECONDS
// One HTTPS fetch of https://HOST:PORT/ through the real fetch_once and the real tls.cpp (built by the script with a
// throwaway trust anchor, see WEATHER_TEST_ANCHORS), with a fixed clock. Prints one line: RESULT err=NAME detail=NAME|N
#include "core/fetch.hpp"
#include "core/log.hpp"
#include "platform/posix/net_posix.hpp"
#include "tls/tls.hpp"
#include <bearssl.h>
#include <cstdio>
#include <cstdlib>

using namespace wx;

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
	if (argc != 4) { fprintf(stderr, "usage: tls_harness HOST PORT CLOCK_UNIX_SECONDS\n"); return 2; }
	log_init(LogError, false, "tls_harness");
	g_clock = atoll(argv[3]);
	Net net;
	posix_net_init(&net);
	tls_net_init(&net);
	net.now = fixed_now;
	FetchOpts o;
	o.timeout_s = 5;
	o.attempts = 1;
	static char buf[4096];
	sv body;
	Status s = fetch_once(net, argv[1], argv[2], true, "/", o, buf, sizeof buf, &body);
	const char *dn = x509_name(s.detail);
	if (dn) printf("RESULT err=%s detail=%s\n", err_name(s.err), dn);
	else printf("RESULT err=%s detail=%d\n", err_name(s.err), s.detail);
	return 0;
}
