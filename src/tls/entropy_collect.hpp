// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// First-run entropy collection (#120): conditioner, credit-gated collector, typed-hex input and
// the seed provenance record. Pure and host-testable; no UI, no timing sources, no storage.
//
// NO CREDIT OR THRESHOLD IS ESTABLISHED. #121 has measured nothing that supports crediting a bit,
// so Policy::unestablished() credits zero and can never complete: the collector fails closed until a
// reviewed policy built from the #121 data replaces it. Tests use explicit synthetic policies.
#pragma once
#include <bearssl.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "secret_clear.hpp"

namespace wx {

// ---- Seed provenance (sidecar record; the 72-byte WXSEED1 seed record is unchanged) ----
enum class SeedOrigin : unsigned char { Unknown = 0, Provisioned = 1, Collected = 2 };
constexpr size_t kProvRecordSize = 16;
inline void prov_encode(SeedOrigin kind, unsigned char record[kProvRecordSize])
{
	std::memset(record, 0, kProvRecordSize);
	std::memcpy(record, "WXPROV1\n", 8);
	record[8] = static_cast<unsigned char>(kind);
}
inline bool prov_decode(const unsigned char *record, size_t len, SeedOrigin &kind)
{
	if (!record || len != kProvRecordSize || std::memcmp(record, "WXPROV1\n", 8) != 0) return false;
	for (size_t i = 9; i < kProvRecordSize; ++i) if (record[i]) return false;
	if (record[8] != 1 && record[8] != 2) return false;
	kind = static_cast<SeedOrigin>(record[8]);
	return true;
}
// No sidecar is a legacy Linux-provisioned install. A sidecar that exists but does not parse
// is Unknown, never silently Provisioned: callers show it and do not upgrade it.
inline SeedOrigin prov_resolve(bool present, const unsigned char *record, size_t len)
{
	if (!present) return SeedOrigin::Provisioned;
	SeedOrigin k;
	return prov_decode(record, len, k) ? k : SeedOrigin::Unknown;
}
// A provisioned seed always wins; a collected seed is never replaced except by explicit regeneration.
inline bool collection_may_replace(SeedOrigin existing, bool explicit_regenerate)
{
	return existing == SeedOrigin::Collected ? explicit_regenerate : false;
}

// ---- Weak-seed screen for collected seeds (#59 policy) ----
// Rejects all-zero, one repeated byte, and constant-step byte sequences (0x01..0x20 fixtures).
inline bool seed_is_weak(const unsigned char seed[32])
{
	unsigned diff_or = 0;
	for (size_t i = 1; i < 32; ++i) diff_or |= static_cast<unsigned char>(seed[i] - seed[0]);
	bool same = diff_or == 0;
	unsigned char step = static_cast<unsigned char>(seed[1] - seed[0]);
	bool progression = true;
	for (size_t i = 2; i < 32; ++i)
		if (static_cast<unsigned char>(seed[i] - seed[i - 1]) != step) progression = false;
	return same || progression;
}

// ---- Collector ----
struct Policy {
	uint32_t event_credit_mbits;   // credited per accepted discrete event (1000 = 1 bit)
	uint32_t threshold_mbits;      // total needed; 0 never completes
	static Policy unestablished() { return Policy{0, 0xFFFFFFFFu}; }
};
enum class EventKind : unsigned char { Key = 1, Click = 2, Motion = 3 };   // Motion is hashed, never credited
enum class HexResult { Ok, Empty, BadChar, TooLong, Junk, Repeat };
constexpr uint32_t kHexDigitCreditMbits = 4000;    // user-attested, full strength (4 bits/digit)
constexpr size_t kHexMaxDigits = 32;
constexpr size_t kHexMaxChars = 128;               // total input incl. separators
constexpr unsigned kHealthWindow = 16;

class Collector {
	Policy policy_;
	br_sha256_context h_;
	uint64_t credit_ = 0;
	uint32_t last_t_ = 0, last_delta_ = 0, deltas_[kHealthWindow] = {};
	unsigned seen_ = 0, events_ = 0;
	bool have_t_ = false, hex_done_ = false, finished_ = false;

	void absorb(unsigned char tag, const void *data, size_t len)
	{
		unsigned char hdr[5] = {tag, static_cast<unsigned char>(len >> 24), static_cast<unsigned char>(len >> 16),
			static_cast<unsigned char>(len >> 8), static_cast<unsigned char>(len)};
		br_sha256_update(&h_, hdr, sizeof hdr);
		if (len) br_sha256_update(&h_, data, len);
	}
	void add_credit(uint64_t mbits) { credit_ += mbits; if (credit_ > 0xFFFFFFFFull) credit_ = 0xFFFFFFFFull; }
public:
	explicit Collector(const Policy &p) : policy_(p)
	{
		br_sha256_init(&h_);
		static const char personalization[] = "weather first-run entropy v1";
		absorb(0, personalization, sizeof personalization - 1);
	}
	~Collector() { secret_clear(&h_, sizeof h_); }
	Collector(const Collector &) = delete;
	Collector &operator=(const Collector &) = delete;

	// Every event is hashed. Only Key/Click earn credit, and only if the health test passes:
	// the interval must be nonzero, must differ from the previous interval (a constant or
	// linear-ramp feeder fails), and must not repeat any of the last kHealthWindow intervals
	// (periodic input fails). This is a guard against scripted input, not an entropy estimate.
	bool add_event(EventKind kind, uint32_t t_us)
	{
		if (finished_) return false;
		unsigned char rec[5] = {static_cast<unsigned char>(kind), static_cast<unsigned char>(t_us >> 24),
			static_cast<unsigned char>(t_us >> 16), static_cast<unsigned char>(t_us >> 8), static_cast<unsigned char>(t_us)};
		absorb(1, rec, sizeof rec);
		if (kind != EventKind::Key && kind != EventKind::Click) return false;
		bool ok = false;
		if (have_t_) {
			uint32_t delta = t_us - last_t_;     // wraps correctly
			ok = delta != 0 && seen_ != 0 && delta != last_delta_;   // the first interval only establishes a baseline
			for (unsigned i = 0; ok && i < seen_ && i < kHealthWindow; ++i) if (deltas_[i] == delta) ok = false;
			deltas_[events_ % kHealthWindow] = delta; last_delta_ = delta;
			if (seen_ < kHealthWindow) ++seen_;
			++events_;
			if (ok) add_credit(policy_.event_credit_mbits);
		}
		last_t_ = t_us; have_t_ = true;
		return ok;
	}

	// Typed or pasted hex from another computer. Spaces, '-' and case are ignored. Nothing is
	// absorbed or credited unless the whole input is accepted; once accepted, a second paste is refused.
	HexResult add_hex(const char *text, size_t len, uint32_t typing_t_us)
	{
		if (finished_ || hex_done_) return HexResult::Repeat;
		if (!text || !len) return HexResult::Empty;
		if (len > kHexMaxChars) return HexResult::TooLong;
		unsigned char nib[kHexMaxDigits];
		size_t n = 0;
		for (size_t i = 0; i < len; ++i) {
			char c = text[i];
			int v;
			if (c == ' ' || c == '-') continue;
			else if (c >= '0' && c <= '9') v = c - '0';
			else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
			else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
			else { secret_clear(nib, sizeof nib); return HexResult::BadChar; }
			if (n == kHexMaxDigits) { secret_clear(nib, sizeof nib); return HexResult::TooLong; }
			nib[n++] = static_cast<unsigned char>(v);
		}
		if (!n) return HexResult::Empty;
		if (n >= 8) {   // one repeated digit, short repeating patterns (period <= 8), constant-step runs
			bool junk = false;
			for (size_t period = 1; period <= 8 && 2 * period <= n && !junk; ++period) {
				bool rep = true;
				for (size_t i = period; i < n; ++i) if (nib[i] != nib[i - period]) { rep = false; break; }
				junk = rep;
			}
			unsigned char step = static_cast<unsigned char>((nib[1] - nib[0]) & 15);
			bool prog = true;
			for (size_t i = 2; i < n; ++i) if (static_cast<unsigned char>((nib[i] - nib[i - 1]) & 15) != step) { prog = false; break; }
			if (junk || prog) { secret_clear(nib, sizeof nib); return HexResult::Junk; }
		} else if (n >= 2) {
			bool same = true;
			for (size_t i = 1; i < n; ++i) if (nib[i] != nib[0]) same = false;
			if (same) { secret_clear(nib, sizeof nib); return HexResult::Junk; }
		}
		absorb(2, nib, n);
		unsigned char t[4] = {static_cast<unsigned char>(typing_t_us >> 24), static_cast<unsigned char>(typing_t_us >> 16),
			static_cast<unsigned char>(typing_t_us >> 8), static_cast<unsigned char>(typing_t_us)};
		absorb(3, t, sizeof t);                   // typing timing is mixed in at zero credit
		add_credit(static_cast<uint64_t>(n) * kHexDigitCreditMbits);
		hex_done_ = true;
		secret_clear(nib, sizeof nib);
		return HexResult::Ok;
	}

	uint32_t credit_mbits() const { return static_cast<uint32_t>(credit_); }
	bool complete() const
	{
		return !finished_ && policy_.threshold_mbits != 0 && credit_ >= policy_.threshold_mbits;
	}
	// Writes the 32-byte seed only when complete and not weak; the collector is spent either way
	// it succeeds. On refusal nothing is written to `seed`.
	bool finish(unsigned char seed[32])
	{
		if (!complete()) return false;
		unsigned char out[32];
		br_sha256_out(&h_, out);
		finished_ = true;
		secret_clear(&h_, sizeof h_);
		bool weak = seed_is_weak(out);
		if (!weak) std::memcpy(seed, out, 32);
		secret_clear(out, sizeof out);
		return !weak;
	}
};
} // namespace wx
