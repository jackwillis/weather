// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Actual OS 9 store failure tests, confined to a newly created disposable folder.
#include "platform/os9/seed_store_os9.hpp"
#include "tls/seed_pool.hpp"
#include <Events.h>
#include <cstdio>
#include <cstring>

namespace {
FILE *logfile;
int failures;
void check(bool ok, const char *label)
{
	if (!ok) ++failures;
	std::fprintf(logfile, "%s %s\n", ok ? "ok" : "FAIL", label);
	std::fflush(logfile);
}
OSErr spec(const char *path, FSSpec &out)
{
	unsigned char p[256]; size_t n = std::strlen(path);
	if (n > 255) return paramErr;
	p[0] = static_cast<unsigned char>(n); std::memcpy(p + 1, path, n);
	return FSMakeFSSpec(0, 0, p, &out);
}
void cleanup(const char *folder)
{
	const char *names[] = {"seed-test-primary", "seed-test-next", "seed-test-lock"};
	for (const char *name : names) {
		char path[256]; std::snprintf(path, sizeof path, "%s:%s", folder, name);
		FSSpec s; OSErr e = spec(path, s);
		if (e == noErr) check(FSpDelete(&s) == noErr, "remove owned test artifact");
		else check(e == fnfErr, "owned test artifact absent");
	}
}
}

int main()
{
	logfile = std::fopen("MacOS9Lives:weather-dev:seed-store-probe.log", "w");
	if (!logfile) return 2;
	char folder[200], primary[256];
	std::snprintf(folder, sizeof folder, "MacOS9Lives:weather-dev:seed-check-%lu", static_cast<unsigned long>(TickCount()));
	FSSpec dir; OSErr e = spec(folder, dir); long id;
	bool owned = e == fnfErr && FSpDirCreate(&dir, 0, &id) == noErr;
	check(owned, "create fresh disposable test directory");
	if (!owned) { std::fclose(logfile); return 1; }
	std::snprintf(primary, sizeof primary, "%s:seed-test-primary", folder);
	char import_path[256]; std::snprintf(import_path, sizeof import_path, "%s:seed-test-import", folder);
	FILE *import = std::fopen(import_path, "wb");
	check(import != nullptr, "create synthetic import for cleanup regression");
	if (import) {
		std::fputs("synthetic import, not a seed", import); std::fclose(import);
		check(wx::os9_delete_file(import_path), "import cleanup deletes the app-owned file");
		FSSpec leftover; OSErr removed = spec(import_path, leftover);
		check(removed == fnfErr, "import path no longer exists");
		// Remove only our disposable fixture if the cleanup under test failed.
		if (removed == noErr) FSpDelete(&leftover);
	}
	// The installer uses a relative import name; cover that path convention too.
	char relative[32]; std::snprintf(relative, sizeof relative, "seed-cleanup-%lu", static_cast<unsigned long>(TickCount()));
	FSSpec local;
	bool made_local = spec(relative, local) == fnfErr && FSpCreate(&local, 'Wthr', 'BINA', 0) == noErr;
	check(made_local, "exclusively create relative cleanup fixture");
	if (made_local) {
		check(wx::os9_delete_file(relative), "relative import cleanup succeeds");
		FSSpec leftover; OSErr removed = spec(relative, leftover);
		check(removed == fnfErr, "relative import no longer exists");
		if (removed == noErr) FSpDelete(&leftover);
	}
	unsigned char seed[32], record[wx::kSeedRecordSize];
	for (size_t i = 0; i < sizeof seed; ++i) seed[i] = static_cast<unsigned char>(i + 1);
	wx::seed_encode(seed, record);
	for (int fault = 0; fault <= 7; ++fault) {
		std::fprintf(logfile, "case fault=%d\n", fault);
		wx::Os9SeedStore store(primary);
		check(store.acquire(), "acquire fixture lock");
		check(store.install(record, sizeof record), "install synthetic fixture");
		check(!store.install(record, sizeof record), "installer never overwrites existing record");
		check(store.release(), "release fixture lock");
		store.set_fault(static_cast<wx::SeedFileFault>(fault));
		wx::SeedPool<wx::Os9SeedStore> pool(store);
		unsigned char out[64]; std::memset(out, 0xA5, sizeof out);
		bool ok = pool.fill(out, sizeof out);
		check(ok == (fault == 0), "TLS entropy success only after full persistence");
		if (fault) {
			bool untouched = true;
			for (unsigned char c : out) untouched &= c == 0xA5;
			check(untouched, "failed rotation released no bytes");
		} else {
			wx::Os9SeedStore rival(primary);
			check(!rival.acquire(), "second store refuses held lock");
		}
		check(pool.shutdown(), "release pool lock");
		wx::Os9SeedStore inspect(primary); unsigned char saved[wx::kSeedRecordSize + 1], decoded[32]; size_t n = 0;
		check(inspect.acquire(), "inspect without live pool");
		bool read_ok = inspect.read(saved, sizeof saved, n);
		check(read_ok && wx::seed_decode(saved, n, decoded), "primary remains a complete valid record");
		if (read_ok && n == sizeof record) {
			bool changed = std::memcmp(saved, record, sizeof record) != 0;
			check(changed == (fault == 0 || fault >= 5), "pre/post-exchange recovery state matches boundary");
		}
		check(inspect.release(), "release inspection lock");
		if (!fault) {
			unsigned char again[64]; wx::SeedPool<wx::Os9SeedStore> restarted(inspect);
			check(restarted.fill(again, sizeof again), "simulated next launch succeeds");
			check(std::memcmp(again, out, sizeof out) != 0, "next launch consumes advanced seed");
			check(restarted.shutdown(), "next launch releases lock");
		}
		cleanup(folder);
	}
	check(FSpDelete(&dir) == noErr, "remove disposable test directory");
	std::fprintf(logfile, "seed store probe finished: %d failed\n", failures);
	std::fclose(logfile);
	return failures ? 1 : 0;
}
