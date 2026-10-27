// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Harness for test/tls_local.sh: tls_harness HOST PORT CLOCK_UNIX_SECONDS [DEADLINE_S]
// One HTTPS fetch of https://HOST:PORT/ through the real fetch_once and the real tls.cpp (built by the script with a
// throwaway trust anchor, see WEATHER_TEST_ANCHORS), with a clock that starts at the given time. Prints one line: RESULT err=NAME detail=NAME|N
#include "core/fetch.hpp"
#include "core/log.hpp"
#include "platform/posix/net_posix.hpp"
#include "tls/tls.hpp"
#include <bearssl.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>

using namespace wx;

// The clock starts at the requested time and then runs in real time, so certificate checks see the chosen date while the
// fetch deadline sees time pass.
static long long g_clock;
static time_t g_started;
static long long fixed_now(Net &) { return g_clock + static_cast<long long>(time(nullptr) - g_started); }

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
	if (argc != 4 && argc != 5) { fprintf(stderr, "usage: tls_harness HOST PORT CLOCK_UNIX_SECONDS [DEADLINE_S]\n"); return 2; }
	alarm(40);                      // safety net: a hang fails the test (no RESULT line) instead of stalling it
	log_init(LogError, false, "tls_harness");
	g_clock = atoll(argv[3]);
	g_started = time(nullptr);
	Net net;
	posix_net_init(&net);
	tls_net_init(&net);
	net.now = fixed_now;
	FetchOpts o;
	o.timeout_s = 5;
	o.attempts = 1;
	if (argc == 5) o.deadline_s = atoi(argv[4]);
	static char buf[4096];
	sv body;
	Status s = fetch_once(net, argv[1], argv[2], true, "/", o, buf, sizeof buf, &body);
	const char *dn = s.err == Err::Timeout && s.detail == ETIMEDOUT ? "ETIMEDOUT" : x509_name(s.detail);
	if (dn) printf("RESULT err=%s detail=%s\n", err_name(s.err), dn);
	else printf("RESULT err=%s detail=%d\n", err_name(s.err), s.detail);
	return 0;
}
