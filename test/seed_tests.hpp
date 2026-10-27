// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Synthetic deterministic seeds only. Never use these fixtures in an installation.
#include "tls/seed_pool.hpp"

struct SeedFake {
	unsigned char disk[wx::kSeedRecordSize + 1] = {};
	size_t len = wx::kSeedRecordSize;
	bool locked = false, fail_lock = false, fail_read = false, fail_replace = false, fail_unlock = false;
	int saves = 0;
	SeedFake() { unsigned char seed[32]; for (int i = 0; i < 32; ++i) seed[i] = static_cast<unsigned char>(i + 1); wx::seed_encode(seed, disk); }
	bool acquire() { if (fail_lock || locked) return false; locked = true; return true; }
	bool release() { if (fail_unlock) return false; locked = false; return true; }
	bool read(unsigned char *out, size_t cap, size_t &n) {
		if (fail_read) return false;
		n = len;
		if (len <= cap) std::memcpy(out, disk, len);
		return true;
	}
	bool replace(const unsigned char *in, size_t n) {
		++saves; if (fail_replace) return false;
		std::memcpy(disk, in, n); len = n; return true;
	}
};

static void test_seed_pool()
{
	unsigned char out[64], first[64];
	{ SeedFake f; unsigned char seed[32];
	  CHECK(wx::seed_decode(f.disk, f.len, seed));
	  for (size_t n = 0; n < wx::kSeedRecordSize; ++n) CHECK(!wx::seed_decode(f.disk, n, seed));
	  CHECK(!wx::seed_decode(f.disk, wx::kSeedRecordSize + 1, seed));
	  for (size_t i = 0; i < wx::kSeedRecordSize; ++i) {
		f.disk[i] ^= 1; CHECK(!wx::seed_decode(f.disk, f.len, seed)); f.disk[i] ^= 1;
	  }
	}
	{ SeedFake f; unsigned char old[wx::kSeedRecordSize]; std::memcpy(old, f.disk, sizeof old);
	  wx::SeedPool<SeedFake> pool(f);
	  CHECK(pool.fill(first, sizeof first)); CHECK(f.saves == 1 && f.locked);
	  // Independent Python stdlib HMAC/SHA-256 implementation of SP800-90A,
	  // seed bytes 1..32, reserve 32 bytes for next launch, then generate 64.
	  const char *expected = "d58fbb4ba1bcc8445e57f05052a6344d451d5e0aa675103f429224f258adb3012"
	                         "288f710d14c8273681b0da4db3be03d28060e152de4cd4b9e89ddeaaad9b06c";
	  char hex[129];
	  for (size_t i = 0; i < sizeof first; ++i) std::snprintf(hex + i * 2, 3, "%02x", first[i]);
	  CHECK(std::strcmp(hex, expected) == 0);
	  CHECK(std::memcmp(old, f.disk, sizeof old) != 0);
	  CHECK(pool.fill(out, sizeof out)); CHECK(std::memcmp(out, first, sizeof out) != 0 && f.saves == 1);
	  CHECK(pool.shutdown() && !f.locked);
	  wx::SeedPool<SeedFake> next(f); CHECK(next.fill(out, sizeof out));
	  CHECK(std::memcmp(out, first, sizeof out) != 0 && f.saves == 2); CHECK(next.shutdown());
	}
	{ SeedFake a, b; wx::SeedPool<SeedFake> one(a), two(b);
	  CHECK(one.fill(first, sizeof first) && two.fill(out, sizeof out));
	  CHECK(std::memcmp(first, out, sizeof out) == 0); // reproducibility, not an entropy claim
	  CHECK(one.shutdown() && two.shutdown());
	}
	{ SeedFake f; wx::SeedPool<SeedFake> one(f), two(f);
	  CHECK(one.fill(out, sizeof out)); CHECK(!two.fill(out, sizeof out));
	  CHECK(f.locked && one.fill(out, sizeof out)); CHECK(one.shutdown());
	}
	for (int mode = 0; mode < 5; ++mode) {
		SeedFake f;
		if (mode == 0) f.fail_lock = true;
		if (mode == 1) f.fail_read = true;
		if (mode == 2) f.len = 32; // legacy raw jitter seed is never auto-promoted
		if (mode == 3) f.disk[0] ^= 1;
		if (mode == 4) f.fail_replace = true;
		wx::SeedPool<SeedFake> pool(f); std::memset(out, 0xA5, sizeof out);
		CHECK(!pool.fill(out, sizeof out)); CHECK(out[0] == 0xA5 && out[63] == 0xA5);
		CHECK(!f.locked); CHECK(pool.error() && *pool.error());
		CHECK(!pool.fill(out, sizeof out)); // failed session requires restart
	}
	{ SeedFake f; wx::SeedPool<SeedFake> pool(f);
	  CHECK(!pool.fill(nullptr, 1)); CHECK(!pool.fill(out, 65537)); CHECK(f.saves == 0);
	  CHECK(pool.fill(out, 0) && f.saves == 0); CHECK(pool.fill(out, sizeof out)); CHECK(pool.shutdown());
	}
	{ SeedFake f; wx::SeedPool<SeedFake> pool(f); CHECK(pool.fill(out, sizeof out));
	  f.fail_unlock = true; CHECK(!pool.shutdown()); CHECK(f.locked); }
}
