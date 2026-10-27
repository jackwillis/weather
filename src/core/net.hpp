// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
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
	// Cryptographic random bytes for TLS. False = no source available.
	bool (*entropy)(Net &, void *buf, size_t len) = nullptr;
	// Optional TLS: wrap an open stream (verifying host), and close it again.
	Status (*tls_open)(Net &, Io &raw, const char *host, Io *out) = nullptr;
	void (*tls_close)(Net &, Io &tls) = nullptr;
};

} // namespace wx
