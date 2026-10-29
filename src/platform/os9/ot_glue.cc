// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Our own client-side glue for Open Transport, so the OS 9 build needs no Apple object file. It is written from Apple's public
// Technical Note TN1173 ("Open Transport Asset Tracking") and from behaviour we observed on a real Mac OS 9.2.2:
//  - the application owns a small zeroed "client record"; every asset-creating call passes its address as an extra last argument;
//  - InitOpenTransport checks that Open Transport is present, points three words of the record at private variables, sets a flag
//    byte, then calls InitOpenTransportPriv(flags, &record). flags 3 = application client.
// Only the calls the weather app uses are provided. A call we have not verified on real hardware is not added.
#include <Gestalt.h>
#include <cstdlib>
#include <cstddef>
#include "ot_min.h"

namespace {
// layout of the record as the OS expects to find it (40 bytes; the OS fills in the first words itself)
struct ClientRecord {
	UInt32 owned_by_os[2];
	const void *library_manager;   // +8
	const void *slm_global;        // +12
	const void *exit_patch_rd;     // +16
	UInt32 owned_by_os2[3];
	UInt8 flag;                    // +32 (set to 1 by the glue)
	UInt8 pad[7];
};
// Validate the only ABI exercised so far: 32-bit PowerPC CFM/PEF.
// These assertions do NOT establish compatibility with another OT release.
// Non-PPC host stand-ins exercise lifecycle behavior, not this native ABI.
#if defined(__powerpc__) || defined(__POWERPC__)
static_assert(sizeof(void *) == 4, "Open Transport glue requires 32-bit pointers");
static_assert(sizeof(ClientRecord) == 40, "OT client record layout changed");
static_assert(offsetof(ClientRecord, library_manager) == 8, "OT client record offset changed");
static_assert(offsetof(ClientRecord, slm_global) == 12, "OT client record offset changed");
static_assert(offsetof(ClientRecord, exit_patch_rd) == 16, "OT client record offset changed");
static_assert(offsetof(ClientRecord, flag) == 32, "OT client record flag offset changed");
#endif
ClientRecord g_record;
UInt32 g_library_manager = 0;
UInt32 g_slm_global = 0;
UInt32 g_exit_patch_rd = 0xaafe0700;   // empirical: the value this app has always run with; its meaning is undocumented and whether 0 would do is untested (#63)
bool g_open = false;
void (*g_before_close)() = nullptr;

void close_at_exit() { CloseOpenTransport(); }
}

extern "C" {

OSStatus InitOpenTransport(void)
{
	long resp = 0;
	if (Gestalt('otan', &resp) != noErr || !(resp & 1)) return -3201;   // Open Transport not present
	g_record.library_manager = &g_library_manager;
	g_record.slm_global = &g_slm_global;
	g_record.exit_patch_rd = &g_exit_patch_rd;
	g_record.flag = 1;
	OSStatus e = InitOpenTransportPriv(3, &g_record);
	if (e == noErr && !g_open) {
		g_open = true;
		if (std::atexit(close_at_exit) != 0) {
			CloseOpenTransport(); // undo successful init; never leave an unowned OT client
			return -1;
		}
	}
	return e;
}

void weather_ot_before_close(void (*cleanup)(void)) { g_before_close = cleanup; }

void CloseOpenTransport(void)
{
	if (!g_open) return;
	g_open = false;
	if (g_before_close) g_before_close();
	CloseOpenTransportPriv(&g_record);
}

EndpointRef OTOpenEndpoint(OTConfiguration *cfg, OTOpenFlags oflag, TEndpointInfo *info, OSStatus *err)
{
	return OTOpenEndpointPriv(cfg, oflag, info, err, &g_record);
}

InetSvcRef OTOpenInternetServices(OTConfiguration *cfg, OTOpenFlags oflag, OSStatus *err)
{
	return OTOpenInternetServicesPriv(cfg, oflag, err, &g_record);
}

OSStatus OTCloseProvider(ProviderRef ref)
{
	return OTCloseProviderPriv(ref, &g_record);
}

// Verified by DnsProbe on maul: immediate and delayed completion, close pending query.
OSStatus weather_async_open_internet(WeatherOTNotify proc, void *context)
{
	return OTAsyncOpenInternetServicesPriv(kOTDefaultInternetServicesPath, 0, proc, context, &g_record);
}

}
