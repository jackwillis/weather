// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Fetching with timeouts, bounded retries and provider fallback.
#pragma once
#include "forecast.hpp"
#include "net.hpp"
#include "provider.hpp"

namespace wx {

// Certificates are not checked against a clock earlier than this (2025-01-01).
constexpr long long kMinSaneTime = 1735689600LL;

struct ConditionalForecast { Query query; Forecast forecast; long long fetched_at=-1; };
struct FetchMetadata { int code=0; long long expires=0; char last_modified[64]={}; };

struct FetchOpts {
	bool save_forecasts = true; // MET Norway requires response caching.
	// Conditional forecast identity is checked before it can authorize a MET 304.
	const ConditionalForecast *conditional = nullptr;
	sv if_modified_since;
	FetchMetadata *metadata = nullptr;
	int timeout_s = 15;          // per wait (connect, each read): a server that keeps trickling is stopped by deadline_s
	int attempts = 3;            // tries per provider
	long backoff_ms = 500;       // doubles each retry ...
	long max_backoff_ms = 30000; // ... up to this (also keeps the doubling from overflowing a 32-bit long)
	int deadline_s = 0;          // one attempt, from its start, is given up on after this long (checked before every read and
	                             // write, so the TLS handshake counts); 0 = 3 x timeout_s. POSIX socket waits honor
	                             // this budget too; blocking name lookup is checked only after it returns.
	bool dump = false;           // log request/response text at debug level
	const char *force_host = nullptr;  // testing: plain HTTP to this host/port instead
	const char *force_port = nullptr;
	const char *contact = nullptr;     // e-mail or URL for the User-Agent (some services require it)
};

// GET one URL. On success body points into buf. Does not retry.
Status fetch_once(Net &net, const char *host, const char *port, bool tls, const char *path,
    const FetchOpts &o, char *buf, size_t cap, sv *body);

// fetch_once with retries for transient errors.
Status fetch_retry(Net &net, const char *host, const char *port, bool tls, const char *path,
    const FetchOpts &o, char *buf, size_t cap, sv *body);

// Try providers in order; the first good forecast wins. Otherwise return the
// last attempted source's error (kind, message and detail), or the last skip
// reason if none was attempted. Empty lists return NoSource. Each failure is
// logged at warning level; skipped providers do not hide an attempted failure.
Status fetch_forecast(Net &net, const Provider *const *providers, size_t n, const Query &q,
    const FetchOpts &o, char *buf, size_t cap, Forecast *out);

} // namespace wx
