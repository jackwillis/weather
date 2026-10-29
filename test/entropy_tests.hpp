// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// First-run entropy collector (#120). Synthetic policies and timestamps only: these values are
// test inputs, not measurements, and say nothing about real entropy (#121).
#include "tls/entropy_collect.hpp"

static uint32_t ent_lcg(uint32_t &s) { s = s * 1664525u + 1013904223u; return s >> 8; }

static void test_entropy_collect()
{
	using namespace wx;
	const Policy syn{250, 4000};   // synthetic: 0.25 bit/event, 4 bits to finish
	unsigned char seed[32], other[32];

	{ // Provenance record: round trip, strictness, missing vs corrupt sidecar.
	  unsigned char rec[kProvRecordSize]; SeedOrigin k = SeedOrigin::Unknown;
	  prov_encode(SeedOrigin::Collected, rec);
	  CHECK(prov_decode(rec, sizeof rec, k) && k == SeedOrigin::Collected);
	  prov_encode(SeedOrigin::Provisioned, rec);
	  CHECK(prov_decode(rec, sizeof rec, k) && k == SeedOrigin::Provisioned);
	  for (size_t n = 0; n < sizeof rec; ++n) CHECK(!prov_decode(rec, n, k));
	  CHECK(!prov_decode(rec, sizeof rec + 1, k) && !prov_decode(nullptr, sizeof rec, k));
	  for (size_t i = 0; i < sizeof rec; ++i) { unsigned char c = rec[i]; rec[i] ^= 0x01; CHECK(!prov_decode(rec, sizeof rec, k) || i == 8); rec[i] = c; }
	  rec[8] = 0; CHECK(!prov_decode(rec, sizeof rec, k)); rec[8] = 3; CHECK(!prov_decode(rec, sizeof rec, k));
	  prov_encode(SeedOrigin::Collected, rec);
	  CHECK(prov_resolve(false, nullptr, 0) == SeedOrigin::Unknown);
	  CHECK(prov_resolve(true, rec, sizeof rec) == SeedOrigin::Collected);
	  rec[0] ^= 1; CHECK(prov_resolve(true, rec, sizeof rec) == SeedOrigin::Unknown);
	  CHECK(!collection_may_replace(SeedOrigin::Provisioned, true));
	  CHECK(!collection_may_replace(SeedOrigin::Collected, false));
	  CHECK(collection_may_replace(SeedOrigin::Collected, true));
	  CHECK(!collection_may_replace(SeedOrigin::Unknown, true));
	}
	{ // Text without an explicit trusted-source declaration never earns entropy credit.
	  Collector c(Policy{0, 128000});
	  CHECK(c.add_hex("deadbeefcafebabefeedfacebadc0ffe00", 32, 0) == HexResult::Ok);
	  CHECK(c.credit_mbits() == 0 && !c.complete());
	  std::memset(seed, 0xa5, sizeof seed);
	  CHECK(!c.finish(seed));
	  for (unsigned char b : seed) CHECK(b == 0xa5);
	}

	{ // Weak-seed screen.
	  unsigned char s[32] = {};
	  CHECK(seed_is_weak(s));
	  std::memset(s, 0x5a, 32); CHECK(seed_is_weak(s));
	  for (int i = 0; i < 32; ++i) { s[i] = static_cast<unsigned char>(i + 1); }
	  CHECK(seed_is_weak(s));
	  for (int i = 0; i < 32; ++i) { s[i] = static_cast<unsigned char>(200 - 3 * i); }
	  CHECK(seed_is_weak(s));
	  s[17] ^= 1; CHECK(!seed_is_weak(s));
	}
	{ // The shipped policy credits nothing and can never complete (fail closed until #121).
	  Collector c(Policy::unestablished());
	  uint32_t rng = 7, t = 0;
	  for (int i = 0; i < 100000; ++i) { t += 1000 + ent_lcg(rng) % 50000; c.add_event(EventKind::Key, t); }
	  CHECK(c.credit_mbits() == 0 && !c.complete() && !c.finish(seed));
	  CHECK(c.add_hex("0123456789abcdef0f1e2d3c4b5a6978", 32, 5, HexSource::UserAttestedCsprng) == HexResult::Ok);
	  CHECK(c.credit_mbits() == 128000 && !c.complete() && !c.finish(seed));
	  Collector z(Policy{1000, 0}); for (uint32_t i = 0; i < 100; ++i) z.add_event(EventKind::Key, i * i * 7919);
	  CHECK(!z.complete());
	}
	{ // Provisional filter: constant timestamps/rates and this short cycle are refused.
	  // Synthetic irregular input demonstrates mechanics, not measured entropy.
	  Collector k(syn);
	  for (uint32_t i = 0; i < 20000; ++i) k.add_event(EventKind::Key, 5000 + i * 1000);
	  CHECK(k.credit_mbits() == 0 && !k.complete());
	  Collector idle(syn);
	  for (uint32_t i = 0; i < 20000; ++i) idle.add_event(EventKind::Key, 777);
	  CHECK(idle.credit_mbits() == 0);
	  Collector per(syn); uint32_t t = 0;
	  static const uint32_t cyc[] = {900, 1300, 2100, 1700, 4000, 2500, 3100, 1100};
	  for (uint32_t i = 0; i < 20000; ++i) { t += cyc[i % 8]; per.add_event(EventKind::Click, t); }
	  CHECK(per.credit_mbits() <= 8 * syn.event_credit_mbits && !per.complete());
	  Collector mv(syn); uint32_t rng = 3; t = 0;
	  for (int i = 0; i < 20000; ++i) { t += 1 + ent_lcg(rng) % 9000; mv.add_event(EventKind::Motion, t); }
	  CHECK(mv.credit_mbits() == 0 && !mv.complete());   // motion is hashed, never credited
	  Collector ok(syn); rng = 11; t = 0; int n = 0;
	  while (!ok.complete() && n < 100000) { t += 40000 + ent_lcg(rng) % 400000; ok.add_event(EventKind::Key, t); ++n; }
	  CHECK(ok.complete() && n > 0 && n < 100);
	  CHECK(ok.finish(seed) && !seed_is_weak(seed));
	  CHECK(!ok.finish(other) && !ok.add_event(EventKind::Key, t + 12345));   // spent
	}
	{ // Known filter limits: predictable interval ramps and cycles longer than the window pass.
	  // Only the unestablished zero-credit policy is safe before source/health review (#121).
	  for (unsigned mode = 0; mode < 2; ++mode) {
		Collector disabled(Policy::unestablished()), synthetic(Policy{1000, 128000});
		uint32_t t = 0;
		disabled.add_event(EventKind::Key, t); synthetic.add_event(EventKind::Key, t);
		for (uint32_t i = 0; i < 150; ++i) {
			t += mode ? 1001 + i : 1000 + i % 17;
			disabled.add_event(EventKind::Key, t); synthetic.add_event(EventKind::Key, t);
		}
		std::memset(seed, 0xa5, sizeof seed);
		CHECK(disabled.credit_mbits() == 0 && !disabled.complete() && !disabled.finish(seed));
		for (unsigned char b : seed) CHECK(b == 0xa5);
		CHECK(synthetic.complete());   // documents the filter limit, NOT an entropy claim
	  }
	}
	{ // Mixing and state transitions: unattested hex contributes no credit, even with events.
	  Collector c(Policy{1000, 2000});
	  CHECK(c.add_hex("9f3a27c1", 8, 7) == HexResult::Ok && c.credit_mbits() == 0);
	  CHECK(c.add_hex("9f3a27c1", 8, 7, HexSource::UserAttestedCsprng) == HexResult::Repeat);
	  c.add_event(EventKind::Key, 100); c.add_event(EventKind::Key, 200);
	  CHECK(c.credit_mbits() == 0);
	  c.add_event(EventKind::Key, 500);
	  std::memset(seed, 0xa5, sizeof seed);
	  CHECK(c.credit_mbits() == 1000 && !c.finish(seed));
	  for (unsigned char b : seed) CHECK(b == 0xa5);
	  c.add_event(EventKind::Click, 1000);
	  CHECK(c.credit_mbits() == 2000 && c.finish(seed));
	  std::memset(other, 0xa5, sizeof other);
	  CHECK(!c.finish(other) && !c.add_event(EventKind::Key, 1700));
	  CHECK(c.add_hex("9f3a27c1", 8, 8) == HexResult::Repeat);
	  for (unsigned char b : other) CHECK(b == 0xa5);
	  Collector attested(Policy{1000, 33000});
	  CHECK(attested.add_hex("9f3a27c1", 8, 7, HexSource::UserAttestedCsprng) == HexResult::Ok);
	  CHECK(attested.credit_mbits() == 32000 && !attested.complete());
	  attested.add_event(EventKind::Key, 100); attested.add_event(EventKind::Key, 200);
	  CHECK(attested.credit_mbits() == 32000);  // first interval only establishes baseline
	  attested.add_event(EventKind::Click, 500);
	  CHECK(attested.credit_mbits() == 33000 && attested.finish(seed));
	}

	{ // Hex input: separators/case ignored, proportional credit, junk and malformed input refused.
	  const char *good = "9f3A-27c1 0be4 8d56 71aa-c93e 5b02 d4f8";
	  Collector a(Policy{0, 128000}), b(Policy{0, 128000});
	  CHECK(a.add_hex(good, strlen(good), 1, HexSource::UserAttestedCsprng) == HexResult::Ok && a.credit_mbits() == 128000 && a.complete());
	  CHECK(b.add_hex("9F3A27C10BE48D5671AAC93E5B02D4F8", 32, 1, HexSource::UserAttestedCsprng) == HexResult::Ok);
	  CHECK(a.finish(seed) && b.finish(other) && !std::memcmp(seed, other, 32));   // same digits and timing, same seed
	  Collector c(Policy{0, 128000}); CHECK(c.add_hex("9f3a27c10be48d56", 16, 1, HexSource::UserAttestedCsprng) == HexResult::Ok);
	  CHECK(c.credit_mbits() == 64000 && !c.complete());
	  CHECK(c.add_hex("9f3a27c10be48d56", 16, 1, HexSource::UserAttestedCsprng) == HexResult::Repeat && c.credit_mbits() == 64000);
	  Collector d(syn);
	  CHECK(d.add_hex(nullptr, 5, 0, HexSource::UserAttestedCsprng) == HexResult::Empty && d.add_hex("", 0, 0, HexSource::UserAttestedCsprng) == HexResult::Empty);
	  CHECK(d.add_hex(" - - ", 5, 0, HexSource::UserAttestedCsprng) == HexResult::Empty);
	  CHECK(d.add_hex("12g4", 4, 0, HexSource::UserAttestedCsprng) == HexResult::BadChar && d.add_hex("0x9f3a", 6, 0, HexSource::UserAttestedCsprng) == HexResult::BadChar);
	  CHECK(d.add_hex("9f3a27c10be48d5671aac93e5b02d4f80", 33, 0, HexSource::UserAttestedCsprng) == HexResult::TooLong);
	  char big[400]; std::memset(big, ' ', sizeof big);
	  CHECK(d.add_hex(big, sizeof big, 0, HexSource::UserAttestedCsprng) == HexResult::TooLong);
	  static const char *junk[] = {"00000000", "ffffffffffffffff", "abababababababab", "123123123123123123",
		"abcdabcdabcdabcd", "0123456789abcdef", "fedcba9876543210", "0246802468024680", "11"};
	  for (const char *j : junk) CHECK(d.add_hex(j, strlen(j), 0, HexSource::UserAttestedCsprng) == HexResult::Junk);
	  CHECK(d.credit_mbits() == 0);   // rejected input earns nothing and a later good paste still works
	  CHECK(d.add_hex("9f3a27c1", 8, 0, HexSource::UserAttestedCsprng) == HexResult::Ok && d.credit_mbits() == 32000);
	}
	{ // Conditioner: known answer; every input and its framing change the seed.
	  Collector a(Policy{0, 1}), b(Policy{0, 1}), c(Policy{0, 1}), d(Policy{0, 1});
	  a.add_hex("9f3a27c10be48d56", 16, 7, HexSource::UserAttestedCsprng); b.add_hex("9f3a27c10be48d56", 16, 7, HexSource::UserAttestedCsprng);
	  c.add_hex("9f3a27c10be48d57", 16, 7, HexSource::UserAttestedCsprng); d.add_hex("9f3a27c10be48d56", 16, 8, HexSource::UserAttestedCsprng);
	  unsigned char sa[32], sb[32], sc[32], sd[32];
	  CHECK(a.finish(sa) && b.finish(sb) && c.finish(sc) && d.finish(sd));
	  CHECK(!std::memcmp(sa, sb, 32) && std::memcmp(sa, sc, 32) && std::memcmp(sa, sd, 32));
	  // Independent Python hashlib SHA-256 of tag || uint32be(length) || payload:
	  // tag 0: b"weather first-run entropy v1"; tag 2: one byte per hex nibble;
	  // tag 3: uint32be(7). Literal result, not recomputed with the implementation hash.
	  const unsigned char want[32] = {
		0x5b,0x5c,0xd1,0x49,0x32,0xc1,0xa9,0xa2,0x38,0x35,0x6a,0x7f,0xdd,0xae,0x3c,0xa4,
		0x72,0x1f,0xd9,0xec,0x85,0xc6,0xc5,0x3c,0xf9,0xa7,0x06,0xb6,0x05,0x74,0x75,0xe4
	  };
	  CHECK(!std::memcmp(sa, want, 32));
	  // An event stream with the same credit but different timestamps gives a different seed.
	  Collector e(Policy{1000, 2000}), f(Policy{1000, 2000});
	  uint32_t ta[] = {100, 5100, 9300, 20000, 31111}, tb[] = {100, 5100, 9300, 20000, 31112};
	  for (uint32_t x : ta) e.add_event(EventKind::Key, x);
	  for (uint32_t x : tb) f.add_event(EventKind::Key, x);
	  CHECK(e.complete() && f.complete() && e.finish(sa) && f.finish(sb) && std::memcmp(sa, sb, 32));
	}
	{ // Timestamp wraparound is a valid interval; credit saturates instead of wrapping.
	  Collector w(Policy{1000, 1000});
	  w.add_event(EventKind::Key, 0xFFFFFE00u); w.add_event(EventKind::Key, 0xFFFFFF00u);   // baseline, no credit
	  CHECK(!w.complete() && w.add_event(EventKind::Key, 0x00000200u) && w.complete());   // interval 0x300 across the wrap
	  Collector s(Policy{0xFFFFFFFFu, 0xFFFFFFFFu}); uint32_t t = 0, rng = 1;
	  for (int i = 0; i < 40; ++i) { t += 10000 + ent_lcg(rng) % 90000; s.add_event(EventKind::Key, t); }
	  CHECK(s.credit_mbits() == 0xFFFFFFFFu);
	}
}
