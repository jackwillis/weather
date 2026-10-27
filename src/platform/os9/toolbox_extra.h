// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Toolbox calls that exist in InterfaceLib but are missing from Retro68's free headers. Our own declarations of the
// documented API (Inside Macintosh: Operating System Utilities / Time Manager); checked on a real Mac OS 9.2.2.
#ifndef WEATHER_TOOLBOX_EXTRA_H
#define WEATHER_TOOLBOX_EXTRA_H
#include <MacTypes.h>
extern "C" {
struct UnsignedWide { UInt32 hi; UInt32 lo; };            // big-endian 64-bit counter
void Microseconds(UnsignedWide *microTickCount);

struct MachineLocation {
	Fixed latitude;
	Fixed longitude;
	union { SInt8 dlsDelta; SInt32 gmtDelta; } u;          // dlsDelta = top byte: bit 7 = daylight saving in effect
};
void ReadLocation(MachineLocation *loc);
}
#endif
