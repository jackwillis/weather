// Fetching with timeouts, bounded retries and provider fallback.
#pragma once
#include "forecast.hpp"
#include "net.hpp"
#include "provider.hpp"

namespace wx {

// Certificates are not checked against a clock earlier than this (2025-01-01).
constexpr long long kMinSaneTime = 1735689600LL;

struct FetchOpts {
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

// Try providers in order; the first good forecast wins. With one provider the
// real error is returned, with several NoSource (each failure is logged).
Status fetch_forecast(Net &net, const Provider *const *providers, size_t n, const Query &q,
    const FetchOpts &o, char *buf, size_t cap, Forecast *out);

} // namespace wx
