// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Disposable filesystem capability checks; never opens the real weather seed.
#include <Files.h>
#include <cstdio>
#include <cstring>

namespace {
FILE *logfile;
int failures;
void check(bool ok, const char *label, long code = 0)
{
	if (!ok) ++failures;
	std::fprintf(logfile, "%s %s (%ld)\n", ok ? "ok" : "FAIL", label, code);
	std::fflush(logfile);
}
OSErr spec(const char *path, FSSpec &out)
{
	unsigned char p[256]; size_t n = std::strlen(path);
	if (n > 255) return -50;
	p[0] = static_cast<unsigned char>(n); std::memcpy(p + 1, path, n);
	return FSMakeFSSpec(0, 0, p, &out);
}
bool write(FSSpec &s, const char *bytes)
{
	OSErr e = FSpCreate(&s, 'Wthr', 'BINA', 0);
	check(e == 0, "exclusive create", e); if (e) return false;
	short ref;
	e = FSpOpenDF(&s, fsWrPerm, &ref);
	check(e == 0, "open for writing", e); if (e) return false;
	long n = 4; e = FSWrite(ref, &n, bytes);
	check(e == 0 && n == 4, "write four bytes", e);
	check(FSClose(ref) == 0, "close written file");
	return e == 0;
}
void read(FSSpec &s, const char *expected)
{
	short ref; OSErr e = FSpOpenDF(&s, fsRdPerm, &ref);
	check(e == 0, "open for reading", e); if (e) return;
	char bytes[4]; long n = sizeof bytes; e = FSRead(ref, &n, bytes);
	check(e == 0 && n == 4 && std::memcmp(bytes, expected, 4) == 0, "contents after exchange", e);
	check(FSClose(ref) == 0, "close read file");
}
}

int main()
{
	logfile = std::fopen("MacOS9Lives:weather-dev:seed-file-probe.log", "w");
	if (!logfile) return 2;
	FSSpec a, b;
	OSErr ea = spec("MacOS9Lives:weather-dev:seed-probe-a", a);
	OSErr eb = spec("MacOS9Lives:weather-dev:seed-probe-b", b);
	// Refuse existing files rather than destroying prior test data.
	check(ea == fnfErr && eb == fnfErr, "both disposable paths absent");
	if (!failures && write(a, "old!") && write(b, "new!")) {
		OSErr e = FSpCreate(&a, 'Wthr', 'BINA', 0);
		check(e == dupFNErr, "second create refuses existing lock", e);
		check(FlushVol(nullptr, a.vRefNum) == 0, "flush before exchange");
		e = FSpExchangeFiles(&a, &b); check(e == 0, "exchange supported on this volume", e);
		check(FlushVol(nullptr, a.vRefNum) == 0, "flush after exchange");
		if (!e) { read(a, "new!"); read(b, "old!"); }
		check(FSpDelete(&a) == 0, "remove first disposable file");
		check(FSpDelete(&b) == 0, "remove second disposable file");
	}
	short vref; long dir;
	OSErr e = FindFolder(-32768, 'pref', false, &vref, &dir); // kOnSystemDisk
	check(e == 0, "locate active Preferences folder without creating anything", e);
	std::fprintf(logfile, "Preferences: volume=%d directory=%ld\n", vref, dir);
	std::fprintf(logfile, "seed filesystem probe finished: %d failed\n", failures);
	std::fclose(logfile);
	return failures ? 1 : 0;
}
