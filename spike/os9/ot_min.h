// Minimal hand-written declarations for the slice of Open Transport the weather app needs.
// Retro68's free headers have none, and Apple's headers are not redistributable, so these are our own declarations
// of the documented public API. Layouts and signatures were checked against Apple's OpenTransport.h and
// OpenTptInternet.h (OT SDK 1.3, read locally, never copied here) and by running them on a real Mac OS 9.2.2.
//
// Linking needs two Apple object files that we cannot ship (OpenTransportAppPPC.o and OpenTptInetPPC.o, from Apple's
// Open Transport SDK): they supply the "client record" handshake with the OS (Apple TN1173). Point OT_SDK at them.
#ifndef WEATHER_OT_MIN_H
#define WEATHER_OT_MIN_H
#include <MacTypes.h>

extern "C" {
typedef struct OTConfiguration OTConfiguration;
typedef void *EndpointRef;
typedef void *InetSvcRef;
typedef void *ProviderRef;
typedef long OTResult;
typedef UInt32 OTFlags;
typedef UInt32 OTOpenFlags;

struct TNetbuf { UInt32 maxlen; UInt32 len; UInt8 *buf; };
struct TCall { TNetbuf addr; TNetbuf opt; TNetbuf udata; long sequence; };
struct TBind { TNetbuf addr; UInt32 qlen; };
struct TEndpointInfo { SInt32 addr, options, tsdu, etsdu, connect, discon; UInt32 servtype, flags; };
struct InetAddress { UInt16 fAddressType; UInt16 fPort; UInt32 fHost; UInt8 fUnused[8]; };
struct InetHostInfo { char name[256]; UInt32 addrs[10]; };

OSStatus InitOpenTransport(void);
void CloseOpenTransport(void);
OTConfiguration *OTCreateConfiguration(const char *path);
EndpointRef OTOpenEndpoint(OTConfiguration *cfg, OTOpenFlags oflag, TEndpointInfo *info, OSStatus *err);
InetSvcRef OTOpenInternetServices(OTConfiguration *cfg, OTOpenFlags oflag, OSStatus *err);
OSStatus OTCloseProvider(ProviderRef ref);
OSStatus OTBind(EndpointRef ep, TBind *req, TBind *ret);
OSStatus OTConnect(EndpointRef ep, TCall *snd, TCall *rcv);
OTResult OTSnd(EndpointRef ep, void *buf, unsigned long n, OTFlags flags);
OTResult OTRcv(EndpointRef ep, void *buf, unsigned long n, OTFlags *flags);
OSStatus OTSndOrderlyDisconnect(EndpointRef ep);
OSStatus OTInetStringToAddress(InetSvcRef svc, char *name, InetHostInfo *out);
}
// Entry points exported by Open Transport's shared libraries (OpenTransportLib / OpenTptInternetLib). Each takes the address of the
// application's "client record" as its LAST argument (Apple TN1173); ot_glue.cc supplies the record and the public wrappers above.
extern "C" {
OSStatus InitOpenTransportPriv(UInt32 flags, void *client);
void CloseOpenTransportPriv(void *client);
EndpointRef OTOpenEndpointPriv(OTConfiguration *cfg, OTOpenFlags oflag, TEndpointInfo *info, OSStatus *err, void *client);
InetSvcRef OTOpenInternetServicesPriv(OTConfiguration *cfg, OTOpenFlags oflag, OSStatus *err, void *client);
OSStatus OTCloseProviderPriv(ProviderRef ref, void *client);
}
#define kOTDefaultInternetServicesPath (reinterpret_cast<OTConfiguration *>(-3L))
#endif
