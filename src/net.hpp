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
	void (*sleep_ms)(Net &, long ms) = nullptr;
	long (*now)(Net &) = nullptr;   // Unix seconds
	// Optional TLS: wrap an open stream (verifying host), and close it again.
	Status (*tls_open)(Net &, Io &raw, const char *host, Io *out) = nullptr;
	void (*tls_close)(Net &, Io &tls) = nullptr;
};

} // namespace wx
