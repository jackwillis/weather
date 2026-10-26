// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// OS 9 probe 1: does a Retro68-built PowerPC app start in Classic / Mac OS 9, and what does the system say about itself?
// Shows the result in the console window and appends it to a log file we can read over SSH.
#include <Gestalt.h>
#include <Memory.h>
#include <Events.h>
#include <cstdio>
#include <cstring>

static void say(FILE *log, const char *fmt, long a = 0, long b = 0)
{
	char line[200];
	snprintf(line, sizeof line, fmt, a, b);
	puts(line);
	if (log) { fputs(line, log); fputc('\n', log); fflush(log); }
}

int main()
{
	FILE *log = fopen("MacOS9Lives:weather-dev:probe1.log", "a");
	puts(log ? "probe1: log file opened" : "probe1: could not open the log file (the folder MacOS9Lives:weather-dev must exist)");
	say(log, "probe1 started, ticks=%ld", static_cast<long>(TickCount()));
	long v = 0;
	if (Gestalt(gestaltSystemVersion, &v) == noErr) say(log, "system version (gestaltSystemVersion) = 0x%lx", v);
	long mem = 0;
	if (Gestalt(gestaltPhysicalRAMSize, &mem) == noErr) say(log, "physical RAM = %ld bytes", mem);
	say(log, "free heap = %ld bytes", static_cast<long>(FreeMem()));
	long arch = 0;
	if (Gestalt(gestaltSysArchitecture, &arch) == noErr) say(log, "sys architecture = %ld (1=68k, 2=PowerPC)", arch);
	long ot = 0;
	OSErr e = Gestalt('otan', &ot);                       // Open Transport availability
	say(log, "Gestalt('otan') error=%ld", e);
	say(log, "Open Transport gestalt value = 0x%lx", ot);
	long tcp = 0;
	e = Gestalt('otcl', &tcp);                            // Open Transport "classic" version flags (informational)
	say(log, "Gestalt('otcl') error=%ld value=0x%lx", e, tcp);
	say(log, "probe1 finished (close this window)");
	if (log) fclose(log);
	unsigned long t = TickCount();
	while (TickCount() - t < 60 * 20) { }                 // keep the window up for ~20 seconds
	return 0;
}
