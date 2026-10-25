// OS 9 probe 2: can we reach the network through Open Transport from a Retro68 PPC app?
// Retro68's free headers have no Open Transport, so the small subset we need is declared by hand below
// (signatures written from memory of OpenTransport.h: GUESSES until this probe proves them).
// Linked against Retro68's OpenTransportLib / OpenTptInternetLib import libraries.
#include <Events.h>
#include <MacTypes.h>
#include <cstdio>
#include <cstring>

extern "C" {
typedef void *OTConfigurationRef;
typedef void *EndpointRef;
typedef void *InetSvcRef;
struct TNetbuf { UInt32 maxlen; UInt32 len; UInt8 *buf; };
struct TCall { TNetbuf addr; TNetbuf opt; TNetbuf udata; SInt32 sequence; };
struct InetAddress { UInt16 fAddressType; UInt16 fPort; UInt32 fHost; UInt8 fUnused[8]; };
struct InetHostInfo { char name[256]; UInt32 addrs[10]; };
struct TEndpointInfo { SInt32 addr, options, tsdu, etsdu, connect, discon, servtype, flags; };

OSStatus InitOpenTransportPriv(UInt32 flags, void *client);
void CloseOpenTransportPriv(void *context);
OTConfigurationRef OTCreateConfiguration(const char *path);
EndpointRef OTOpenEndpointPriv(OTConfigurationRef cfg, UInt32 oflag, TEndpointInfo *info, OSStatus *err, void *context);
InetSvcRef OTOpenInternetServicesPriv(OTConfigurationRef cfg, UInt32 oflag, OSStatus *err, void *context);
OSStatus OTBind(EndpointRef ep, TNetbuf *req, TNetbuf *ret);
OSStatus OTConnect(EndpointRef ep, TCall *snd, TCall *rcv);
OSStatus OTSnd(EndpointRef ep, void *buf, UInt32 n, UInt32 flags);
SInt32 OTRcv(EndpointRef ep, void *buf, UInt32 n, UInt32 *flags);
OSStatus OTSndOrderlyDisconnect(EndpointRef ep);
OSStatus OTCloseProviderPriv(void *prov);
OSStatus OTInetStringToAddress(InetSvcRef svc, char *name, InetHostInfo *out);
OSStatus OTSetSynchronous(void *prov);
OSStatus OTSetBlocking(void *prov);
}

// The static glue (OpenTransportAppPPC.o) normally provides __gOTClientRecord: an opaque, zeroed, statically allocated
// block whose ADDRESS is passed as the extra last argument of every *Priv call (Apple TN1173). We allocate a generous one.
static char g_client[1024] __attribute__((aligned(16)));

static const OTConfigurationRef kDefaultInternetServicesPath = reinterpret_cast<OTConfigurationRef>(-3L);

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
	g_log = fopen("MacOS9Lives:weather-dev:probe2.log", "a");
	say("probe2 started");
	say("calling InitOpenTransportPriv");
	void *ctx = g_client;
	OSStatus e = InitOpenTransportPriv(1 /* kInitOTForApplicationMask (guess) */, ctx);
	say("InitOpenTransport -> %ld", e);
	if (e != 0) return 1;

	// DNS: resolve the weather hosts through Open Transport's resolver
	OSStatus err = 0;
	InetSvcRef svc = OTOpenInternetServicesPriv(kDefaultInternetServicesPath, 0, &err, ctx);
	say("OTOpenInternetServices -> %ld (svc %s)", err, 0, 0, 0);
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
	EndpointRef ep = OTOpenEndpointPriv(OTCreateConfiguration("tcp"), 0, &info, &err, ctx);
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
		OTCloseProviderPriv(ep);
	}
	if (svc) OTCloseProviderPriv(svc);
	CloseOpenTransportPriv(ctx);
	say("probe2 finished");
	if (g_log) fclose(g_log);
	unsigned long t = TickCount();
	while (TickCount() - t < 60 * 20) { }
	return 0;
}
