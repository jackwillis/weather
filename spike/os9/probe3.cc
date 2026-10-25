// OS 9 probe 3: can we reach the network through Open Transport from a Retro68 PPC app?
// Uses the public Open Transport API (declared in ot_min.h) linked with Apple's static glue (see ot_min.h / CMakeLists.txt).
// Resolves api.open-meteo.com, then opens a TCP connection to port 443 and sends a few bytes (no TLS yet).
#include <Events.h>
#include <MacTypes.h>
#include <cstdio>
#include <cstring>

#include "ot_min.h"

static FILE *g_log;
static void say(const char *fmt, long a = 0, long b = 0, long c = 0, long d = 0)
{
	char line[200];
	snprintf(line, sizeof line, fmt, a, b, c, d);
	puts(line);
	if (g_log) { fputs(line, g_log); fputc('\n', g_log); fflush(g_log); }
}

int main()
{
	g_log = fopen("MacOS9Lives:weather-dev:probe3.log", "a");
	say("probe3 started");
	say("calling InitOpenTransport");
	OSStatus e = InitOpenTransport();
	say("InitOpenTransport -> %ld", e);
	if (e != 0) return 1;

	// DNS: resolve the weather hosts through Open Transport's resolver
	OSStatus err = 0;
	InetSvcRef svc = OTOpenInternetServices(kOTDefaultInternetServicesPath, 0, &err);
	say("OTOpenInternetServices -> %ld", err);
	InetHostInfo hi;
	UInt32 ip = 0;
	if (svc && err == 0) {
		char name[] = "api.open-meteo.com";
		std::memset(&hi, 0, sizeof hi);
		e = OTInetStringToAddress(svc, name, &hi);
		ip = hi.addrs[0];
		say("OTInetStringToAddress(api.open-meteo.com) -> %ld, first address 0x%lx", e, static_cast<long>(ip));
	}

	// TCP: connect to port 443 and send/receive a few bytes (no TLS yet: expect the server to answer or close)
	TEndpointInfo info;
	err = 0;
	EndpointRef ep = OTOpenEndpoint(OTCreateConfiguration("tcp"), 0, &info, &err);
	say("OTOpenEndpoint(tcp) -> %ld", err);
	if (ep && err == 0) {
		e = OTBind(ep, nullptr, nullptr);
		say("OTBind -> %ld", e);
		if (ip) {
			InetAddress a;
			std::memset(&a, 0, sizeof a);
			a.fAddressType = 2; a.fPort = 443; a.fHost = ip;
			TCall call;
			std::memset(&call, 0, sizeof call);
			call.addr.len = sizeof a; call.addr.maxlen = sizeof a; call.addr.buf = reinterpret_cast<UInt8 *>(&a);
			e = OTConnect(ep, &call, nullptr);
			say("OTConnect(:443) -> %ld", e);
			if (e == 0) {
				const char junk[] = "hello\r\n\r\n";
				e = OTSnd(ep, const_cast<char *>(junk), sizeof junk - 1, 0);
				say("OTSnd -> %ld", e);
				char buf[64];
				UInt32 fl = 0;
				long n = OTRcv(ep, buf, sizeof buf, &fl);
				say("OTRcv -> %ld (negative = error)", n);
				OTSndOrderlyDisconnect(ep);
			}
		}
		OTCloseProvider(ep);
	}
	if (svc) OTCloseProvider(svc);
	CloseOpenTransport();
	say("probe3 finished");
	if (g_log) fclose(g_log);
	unsigned long t = TickCount();
	while (TickCount() - t < 60 * 20) { }
	return 0;
}
