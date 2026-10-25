#include "seed_store_os9.hpp"
#include <cstring>

namespace wx {
namespace {
OSErr make_spec(short volume, long directory, const char *name, FSSpec &out)
{
	size_t n = std::strlen(name);
	if (n > 255) return paramErr;
	unsigned char p[256]; p[0] = static_cast<unsigned char>(n);
	std::memcpy(p + 1, name, n);
	return FSMakeFSSpec(volume, directory, p, &out);
}
bool usable(OSErr e) { return e == noErr || e == fnfErr; }
}

bool Os9SeedStore::init()
{
	if (initialized_) return true;
	short volume = 0; long directory = 0;
	if (test_path_) {
		if (!usable(make_spec(0, 0, test_path_, primary_))) return false;
		volume = primary_.vRefNum; directory = primary_.parID;
		// These names are reserved for the diagnostic, not the real store.
		if (!usable(make_spec(volume, directory, "seed-test-next", next_)) ||
		    !usable(make_spec(volume, directory, "seed-test-lock", lock_))) return false;
	} else {
		if (FindFolder(-32768, 'pref', false, &volume, &directory) != noErr) return false;
		if (!usable(make_spec(volume, directory, "Weather TLS Seed", primary_)) ||
		    !usable(make_spec(volume, directory, "Weather TLS Next", next_)) ||
		    !usable(make_spec(volume, directory, "Weather TLS Lock", lock_))) return false;
	}
	initialized_ = true;
	return true;
}

bool Os9SeedStore::acquire()
{
	if (held_ || !init()) return false;
	// FSpCreate returns dupFNErr for an existing file; never steal a lock.
	held_ = FSpCreate(&lock_, 'Wthr', 'BINA', 0) == noErr;
	return held_;
}

bool Os9SeedStore::release()
{
	if (!held_) return true;
	if (FSpDelete(&lock_) != noErr) return false;
	held_ = false;
	return true;
}

bool Os9SeedStore::read(unsigned char *data, size_t cap, size_t &n)
{
	if (!held_) return false;
	short ref;
	if (FSpOpenDF(&primary_, fsRdPerm, &ref) != noErr) return false;
	long len = 0; OSErr e = GetEOF(ref, &len);
	if (e == noErr && len >= 0) {
		n = static_cast<size_t>(len);
		if (n <= cap) {
			long count = len; e = FSRead(ref, &count, data);
			if (count != len) e = eofErr;
		}
	} else if (e == noErr) e = paramErr;
	OSErr closed = FSClose(ref);
	return e == noErr && closed == noErr;
}

bool Os9SeedStore::write_new(FSSpec &spec, const unsigned char *data, size_t n)
{
	if (n > 65536 || FSpCreate(&spec, 'Wthr', 'BINA', 0) != noErr) return false;
	short ref;
	OSErr e = FSpOpenDF(&spec, fsWrPerm, &ref);
	if (e == noErr) {
		long count = static_cast<long>(n);
#ifdef WEATHER_SEED_DIAGNOSTICS
		if (fault_ == SeedFileFault::ShortWrite && count) --count;
#endif
		e = FSWrite(ref, &count, data);
		if (count != static_cast<long>(n)) e = ioErr;
		OSErr closed = FSClose(ref);
#ifdef WEATHER_SEED_DIAGNOSTICS
		if (fault_ == SeedFileFault::Close) closed = ioErr;
#endif
		if (e == noErr) e = closed;
	}
	if (e == noErr) {
#ifdef WEATHER_SEED_DIAGNOSTICS
		if (fault_ == SeedFileFault::FlushBefore) e = ioErr;
		else
#endif
		e = FlushVol(nullptr, spec.vRefNum);
	}
	if (e != noErr) {
		// Delete only the file this call created. Failure leaves a visible recovery
		// artifact; subsequent attempts never overwrite it.
		FSpDelete(&spec);
		return false;
	}
	return true;
}

bool Os9SeedStore::replace(const unsigned char *data, size_t n)
{
	if (!held_ || !write_new(next_, data, n)) return false;
#ifdef WEATHER_SEED_DIAGNOSTICS
	if (fault_ == SeedFileFault::Exchange) return false;
#endif
	if (FSpExchangeFiles(&primary_, &next_) != noErr) return false;
	// Do not release output until replacement and flushing succeeded. A leftover
	// Next file is deliberately not auto-recovered after an ambiguous failure.
#ifdef WEATHER_SEED_DIAGNOSTICS
	if (fault_ == SeedFileFault::FlushAfter) return false;
#endif
	if (FlushVol(nullptr, primary_.vRefNum) != noErr) return false;
#ifdef WEATHER_SEED_DIAGNOSTICS
	if (fault_ == SeedFileFault::DeleteNext) return false;
#endif
	if (FSpDelete(&next_) != noErr) return false;
#ifdef WEATHER_SEED_DIAGNOSTICS
	if (fault_ == SeedFileFault::FlushFinal) return false;
#endif
	return FlushVol(nullptr, primary_.vRefNum) == noErr;
}

bool Os9SeedStore::install(const unsigned char *data, size_t n)
{
	return held_ && write_new(primary_, data, n);
}
}
