// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Error kinds. Every fallible call returns a Status (no exceptions, no heap).
#pragma once
#include <cstddef>

namespace wx {

enum class Err {
	Ok = 0,
	Usage,         // bad command line
	Config,        // bad config file (detail = line number)
	Clock,         // system clock obviously wrong; certificates cannot be checked
	Dns,           // name lookup failed
	Connect,       // TCP connect failed
	Timeout,       // an I/O step took too long
	Io,            // read/write failed or connection dropped
	Tls,           // TLS handshake/protocol failure (detail = BearSSL error)
	CertUntrusted, // certificate rejected (detail = BearSSL error)
	HttpStatus,    // server answered with a non-200 (detail = status)
	Truncated,     // response shorter than it claimed to be
	Parse,         // malformed HTTP or JSON
	BadData,       // well-formed but missing/implausible fields
	NoSource,      // every data source failed
	Cancelled,     // the caller (user, signal) asked us to stop
};

struct Status {
	Err err = Err::Ok;
	int detail = 0;        // errno, HTTP status, BearSSL code, line number...
	const char *msg = "";  // static string only
	constexpr bool ok() const { return err == Err::Ok; }
};

constexpr Status make(Err e, const char *msg, int detail = 0) { return Status{e, detail, msg}; }
constexpr Status okay() { return Status{}; }

constexpr const char *err_name(Err e)
{
	switch (e) {
	case Err::Ok: return "ok";
	case Err::Usage: return "usage";
	case Err::Config: return "config";
	case Err::Clock: return "clock";
	case Err::Dns: return "dns";
	case Err::Connect: return "connect";
	case Err::Timeout: return "timeout";
	case Err::Io: return "io";
	case Err::Tls: return "tls";
	case Err::CertUntrusted: return "cert";
	case Err::HttpStatus: return "http";
	case Err::Truncated: return "truncated";
	case Err::Parse: return "parse";
	case Err::BadData: return "baddata";
	case Err::NoSource: return "nosource";
	case Err::Cancelled: return "cancelled";
	}
	return "?";
}

// BSD sysexits.h values.
constexpr int exit_code(Err e)
{
	switch (e) {
	case Err::Ok: return 0;
	case Err::Usage: return 64;       // EX_USAGE
	case Err::Config: return 78;      // EX_CONFIG
	case Err::Clock: return 71;       // EX_OSERR
	case Err::Dns: return 68;         // EX_NOHOST
	case Err::Connect: return 69;     // EX_UNAVAILABLE
	case Err::Timeout: return 75;     // EX_TEMPFAIL
	case Err::Io: return 74;          // EX_IOERR
	case Err::Tls: return 76;         // EX_PROTOCOL
	case Err::CertUntrusted: return 76;
	case Err::HttpStatus: return 76;
	case Err::Truncated: return 74;
	case Err::Parse: return 65;       // EX_DATAERR
	case Err::BadData: return 65;
	case Err::NoSource: return 69;
	case Err::Cancelled: return 130;  // 128 + SIGINT, the shell convention
	}
	return 70;                        // EX_SOFTWARE
}

// Worth trying again later?
constexpr bool retryable(const Status &s)
{
	switch (s.err) {
	case Err::Dns: case Err::Connect: case Err::Timeout:
	case Err::Io: case Err::Truncated:
		return true;
	case Err::HttpStatus:
		return s.detail >= 500 || s.detail == 429;
	default:
		return false;
	}
}

} // namespace wx
