// Exercise actual glue, including libc's real LIFO atexit path. No Apple files.
#include "platform/os9/ot_min.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
static bool open_ot, endpoint;
static bool fail_init;
static void require(bool ok) { if (!ok) std::abort(); }
extern "C" OSStatus Gestalt(UInt32, long *out) { *out = 1; return 0; }
extern "C" OSStatus InitOpenTransportPriv(UInt32, void *) { open_ot = !fail_init; return fail_init ? -1 : 0; }
extern "C" void CloseOpenTransportPriv(void *) {
	require(open_ot && !endpoint); open_ot = false; std::puts("transport closed");
}
extern "C" EndpointRef OTOpenEndpointPriv(OTConfiguration *, OTOpenFlags, TEndpointInfo *, OSStatus *, void *) {
	require(open_ot); endpoint = true; return &endpoint;
}
extern "C" InetSvcRef OTOpenInternetServicesPriv(OTConfiguration *, OTOpenFlags, OSStatus *, void *) { return nullptr; }
extern "C" OSStatus OTAsyncOpenInternetServicesPriv(OTConfiguration *, OTOpenFlags, WeatherOTNotify, void *, void *) { return -1; }
extern "C" OSStatus OTCloseProviderPriv(ProviderRef, void *) {
	require(open_ot && endpoint); endpoint = false; std::puts("endpoint closed"); return 0;
}
static void cleanup() {
	if (endpoint) require(OTCloseProvider(&endpoint) == 0);
	// Prove a reentrant close cannot close the transport twice.
	CloseOpenTransport();
}
static void older_exit_handler() { cleanup(); }
int main(int argc, char **argv) {
	require(argc == 2);
	fail_init = std::strcmp(argv[1], "failed-init") == 0;
	require(std::atexit(older_exit_handler) == 0); // net cleanup registered before lazy OT init
	weather_ot_before_close(cleanup);
	OSStatus s = InitOpenTransport();
	if (fail_init) { require(s != 0); CloseOpenTransport(); return 0; }
	require(s == 0);
	OSStatus error = 0;
	require(OTOpenEndpoint(nullptr, 0, nullptr, &error) != nullptr);
	if (std::strcmp(argv[1], "explicit") == 0) { CloseOpenTransport(); CloseOpenTransport(); }
	else require(std::strcmp(argv[1], "exit") == 0);
	return 0;
}
