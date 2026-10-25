#pragma once
#include <Files.h>
#include <cstddef>

namespace wx {
// Remove an app-owned file using the same HFS path conventions as fopen.
bool os9_delete_file(const char *path);
#ifdef WEATHER_SEED_DIAGNOSTICS
enum class SeedFileFault { None, ShortWrite, Close, FlushBefore, Exchange, FlushAfter, DeleteNext, FlushFinal };
#endif
// Owns only Weather TLS Seed/Next/Lock in Preferences. Diagnostic paths point
// at separate disposable files; never infer seed provenance from file length.
class Os9SeedStore {
	FSSpec primary_ = {}, next_ = {}, lock_ = {};
	const char *test_path_;
	bool initialized_ = false, held_ = false;
#ifdef WEATHER_SEED_DIAGNOSTICS
	SeedFileFault fault_ = SeedFileFault::None;
#endif
	bool init();
	bool write_new(FSSpec &spec, const unsigned char *data, size_t n);
public:
	explicit Os9SeedStore(const char *diagnostic_path = nullptr) : test_path_(diagnostic_path) {}
	bool acquire();
	bool release();
	bool read(unsigned char *data, size_t cap, size_t &n);
	bool replace(const unsigned char *data, size_t n);
	bool install(const unsigned char *data, size_t n); // exclusive creation; never overwrites
#ifdef WEATHER_SEED_DIAGNOSTICS
	void set_fault(SeedFileFault fault) { fault_ = fault; }
#endif
};
}
