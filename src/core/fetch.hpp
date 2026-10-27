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

struct ConditionalForecast { Query query; Forecast forecast; };
struct FetchMetadata { int code=0; char last_modified[64]={}; };

struct FetchOpts {
	// Conditional forecast identity is checked before it can authorize a MET 304.
	const ConditionalForecast *conditional = nullptr;
	sv if_modified_since;
	FetchMetadata *metadata = nullptr;
	int timeout_s = 15;
	int attempts = 3;            // tries per provider
	long backoff_ms = 500;       // doubles each retry ...
	long max_backoff_ms = 30000; // ... up to this (also keeps the doubling from overflowing a 32-bit long)
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

// Try providers in order; the first good forecast wins. With one provider the
// real error is returned, with several NoSource (each failure is logged).
Status fetch_forecast(Net &net, const Provider *const *providers, size_t n, const Query &q,
    const FetchOpts &o, char *buf, size_t cap, Forecast *out);

} // namespace wx
