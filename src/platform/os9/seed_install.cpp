// Explicit first-install utility. Import file is generated on a trusted modern OS.
#include "seed_store_os9.hpp"
#include "tls/seed_pool.hpp"
#include <cstdio>

int main()
{
	FILE *log = std::fopen("seed-install.log", "w");
	if (!log) return 2;
	unsigned char record[wx::kSeedRecordSize + 1] = {}, seed[32] = {};
	FILE *input = std::fopen("weather-seed.import", "rb");
	size_t n = 0; bool valid = false;
	if (input) {
		n = std::fread(record, 1, sizeof record, input);
		bool read_ok = !std::ferror(input);
		bool close_ok = std::fclose(input) == 0;
		valid = read_ok && close_ok && wx::seed_decode(record, n, seed);
	}
	wx::secret_clear(seed, sizeof seed);
	if (!valid) {
		std::fputs("FAIL: weather-seed.import missing or invalid. Generate it with tools/provision-os9-seed.py on Linux.\n", log);
		wx::secret_clear(record, sizeof record); std::fclose(log); return 1;
	}
	wx::Os9SeedStore store;
	bool locked = store.acquire();
	bool installed = locked && store.install(record, n);
	wx::secret_clear(record, sizeof record);
	bool released = !locked || store.release();
	bool removed = installed && wx::os9_delete_file("weather-seed.import");
	std::fprintf(log, "%s: %s\n", installed && released && removed ? "ok" : "FAIL",
	             !locked ? "seed store busy or a stale lock exists; nothing installed" :
	             !installed ? "seed install failed or a seed already exists; existing seed was not overwritten" :
	             !released ? "seed installed, but lock removal failed; close apps and recover the lock before running Weather" :
	             !removed ? "seed installed, but import deletion failed; securely remove weather-seed.import" :
	             "seed installed in Preferences; import removed. Start Weather to rotate it before TLS use.");
	std::fclose(log);
	return installed && released && removed ? 0 : 1;
}
