// The OS seam. The core talks only to these function pointers.
#pragma once
#include "status.hpp"

namespace wx {

// A byte stream. read: >0 bytes, 0 = clean EOF, <0 = error (see detail/err).
struct Io {
	void *ctx = nullptr;
	long (*read)(Io &, void *buf, size_t len) = nullptr;
	long (*write)(Io &, const void *buf, size_t len) = nullptr;
	Err err = Err::Ok;   // set by read/write when they return <0
	int detail = 0;      // errno or similar
	const char *msg = nullptr;  // optional static reason for the error
};

// Elapsed-time budget, in the Net monotonic clock domain. No timestamp sum.
struct TimeBudget {
	long long start_ms, limit_ms;
	constexpr long long remaining(long long now) const {
		if (start_ms < 0 || limit_ms < 0 || now < start_ms) return -1;
		long long elapsed = now - start_ms;
		return elapsed >= limit_ms ? 0 : limit_ms - elapsed;
	}
};

struct Net {
	void *ctx = nullptr;
	Status (*connect)(Net &, const char *host, const char *port, int timeout_s, Io *out) = nullptr;
	void (*close)(Net &, Io &) = nullptr;
	// Waiting. Whenever the platform would block (reads, writes, connect, backoff sleeps) it
	// wakes about every 50 ms and calls yield, so a UI can pump its event loop. yield returns
	// false to cancel: the operation then fails with Err::Cancelled. Optional.
	bool (*yield)(Net &) = nullptr;
	bool (*sleep_ms)(Net &, long ms) = nullptr;   // false = cancelled while sleeping
	long long (*now)(Net &) = nullptr;            // Unix seconds (64-bit: ppc32 long dies in 2038)
	// Elapsed/active milliseconds, nonnegative and nondecreasing; -1 = unavailable.
	// Separate from UTC: platforms may pause this clock during modal UI tracking.
	long long (*monotonic_ms)(Net &) = nullptr;
	// Cryptographic random bytes for TLS. False = no source available.
	bool (*entropy)(Net &, void *buf, size_t len) = nullptr;
	// Optional TLS: wrap an open stream (verifying host), and close it again.
	Status (*tls_open)(Net &, Io &raw, const char *host, Io *out) = nullptr;
	void (*tls_close)(Net &, Io &tls) = nullptr;
	// Optional borrowed operation budget; backend waits use the earlier of this
	// and their own timeout. Only valid during the operation; never retain it.
	const TimeBudget *budget = nullptr;
};

} // namespace wx
