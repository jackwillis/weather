// Managed seed continuity for systems without an OS CSPRNG. Requires a secret,
// independently provisioned initial seed; this code cannot prove its provenance.
#pragma once
#include <bearssl.h>
#include <cstddef>
#include <cstring>
#include "secret_clear.hpp"

namespace wx {
constexpr size_t kSeedRecordSize = 72;
inline void seed_encode(const unsigned char seed[32], unsigned char record[kSeedRecordSize])
{
	std::memcpy(record, "WXSEED1\n", 8);
	std::memcpy(record + 8, seed, 32);
	br_sha256_context h; br_sha256_init(&h);
	br_sha256_update(&h, record, 40); br_sha256_out(&h, record + 40);
	secret_clear(&h, sizeof h);
}
inline bool seed_decode(const unsigned char *record, size_t len, unsigned char seed[32])
{
	if (!record || len != kSeedRecordSize || std::memcmp(record, "WXSEED1\n", 8) != 0) return false;
	unsigned char digest[32];
	br_sha256_context h; br_sha256_init(&h);
	br_sha256_update(&h, record, 40); br_sha256_out(&h, digest);
	unsigned different = 0, nonzero = 0;
	for (size_t i = 0; i < 32; ++i) {
		different |= digest[i] ^ record[40 + i];
		nonzero |= record[8 + i];
	}
	secret_clear(digest, sizeof digest); secret_clear(&h, sizeof h);
	// This rejects an obvious placeholder, not arbitrary weak or public seeds.
	if (different || !nonzero) return false;
	std::memcpy(seed, record + 8, 32);
	return true;
}

template<class Store> class SeedPool {
	Store &store_;
	br_hmac_drbg_context rng_ = {};
	bool ready_ = false, held_ = false, failed_ = false;
	const char *error_ = "";
	bool fail(const char *reason)
	{
		error_ = reason; failed_ = true; ready_ = false;
		secret_clear(&rng_, sizeof rng_);
		if (held_ && store_.release()) held_ = false;
		return false;
	}
public:
	explicit SeedPool(Store &s) : store_(s) {}
	SeedPool(const SeedPool &) = delete;
	SeedPool &operator=(const SeedPool &) = delete;
	const char *error() const { return error_; }
	bool fill(void *out, size_t len, const void *extra = nullptr, size_t extra_len = 0)
	{
		if ((!out && len) || len > 65536 || (!extra && extra_len)) return false;
		if (failed_) return false;
		if (!len) return true;
		if (!ready_) {
			if (!store_.acquire()) return fail("cannot lock TLS seed store (another app or stale lock)");
			held_ = true;
			unsigned char record[kSeedRecordSize + 1] = {}, seed[32] = {};
			size_t n = 0;
			bool valid = store_.read(record, sizeof record, n) && seed_decode(record, n, seed);
			secret_clear(record, sizeof record);
			if (!valid) { secret_clear(seed, sizeof seed); return fail("TLS seed missing or invalid; provision a new seed and restart"); }
			br_hmac_drbg_init(&rng_, &br_sha256_vtable, seed, sizeof seed);
			secret_clear(seed, sizeof seed);
			if (extra_len) br_hmac_drbg_update(&rng_, extra, extra_len);
			// Reserve the next launch's seed before releasing any TLS bytes.
			br_hmac_drbg_generate(&rng_, seed, sizeof seed);
			seed_encode(seed, record); secret_clear(seed, sizeof seed);
			bool saved = store_.replace(record, kSeedRecordSize);
			secret_clear(record, sizeof record);
			if (!saved) return fail("cannot rotate TLS seed safely; no TLS entropy released");
			ready_ = true;
		} else if (extra_len) br_hmac_drbg_update(&rng_, extra, extra_len);
		br_hmac_drbg_generate(&rng_, out, len);
		return true;
	}
	bool shutdown()
	{
		secret_clear(&rng_, sizeof rng_); ready_ = false; failed_ = true;
		if (held_) {
			if (!store_.release()) { error_ = "cannot release TLS seed lock"; return false; }
			held_ = false;
		}
		return true;
	}
};
} // namespace wx
