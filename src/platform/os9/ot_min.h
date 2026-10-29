// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Minimal hand-written declarations for the slice of Open Transport the weather app needs.
// Retro68's free headers have none, and Apple's headers are not redistributable, so these are our own declarations
// of the documented public API. They follow Apple's published Open Transport documentation and Technical Note TN1173, and
// the declarations in use have been run on a real Mac OS 9.2.2.
//
// Our ot_glue.cc supplies the client-record handshake; no Apple object files are needed.
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
typedef pascal void (*WeatherOTNotify)(void *, UInt32, OTResult, void *);

struct TNetbuf { UInt32 maxlen; UInt32 len; UInt8 *buf; };
struct TCall { TNetbuf addr; TNetbuf opt; TNetbuf udata; long sequence; };
struct TDiscon { TNetbuf udata; SInt32 reason; long sequence; };
struct TBind { TNetbuf addr; UInt32 qlen; };
struct TEndpointInfo { SInt32 addr, options, tsdu, etsdu, connect, discon; UInt32 servtype, flags; };
struct InetAddress { UInt16 fAddressType; UInt16 fPort; UInt32 fHost; UInt8 fUnused[8]; };
struct InetHostInfo { char name[256]; UInt32 addrs[10]; };

OSStatus InitOpenTransport(void);
void CloseOpenTransport(void);
OTConfiguration *OTCreateConfiguration(const char *path);
EndpointRef OTOpenEndpoint(OTConfiguration *cfg, OTOpenFlags oflag, TEndpointInfo *info, OSStatus *err);
InetSvcRef OTOpenInternetServices(OTConfiguration *cfg, OTOpenFlags oflag, OSStatus *err);
OSStatus weather_async_open_internet(WeatherOTNotify proc, void *context);
OSStatus OTCloseProvider(ProviderRef ref);
OSStatus OTBind(EndpointRef ep, TBind *req, TBind *ret);
OSStatus OTConnect(EndpointRef ep, TCall *snd, TCall *rcv);
OSStatus OTRcvConnect(EndpointRef ep, TCall *call);
OSStatus OTRcvDisconnect(EndpointRef ep, TDiscon *discon);
OSStatus OTRcvOrderlyDisconnect(EndpointRef ep);
OSStatus OTSetSynchronous(ProviderRef ref);
OSStatus OTSetNonBlocking(ProviderRef ref);
OTResult OTLook(EndpointRef ep);
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
OSStatus OTAsyncOpenInternetServicesPriv(OTConfiguration *cfg, OTOpenFlags oflag, WeatherOTNotify proc, void *context, void *client);
OSStatus OTCloseProviderPriv(ProviderRef ref, void *client);
}
// Error codes and event codes returned by OTLook, as documented in Apple's Open Transport reference.
enum {
	kOTBadAddressErr = -3150, kOTOutStateErr = -3155, kOTLookErr = -3158, kOTFlowErr = -3161, kOTNoDataErr = -3162,
	kOTCanceledErr = -3180, kOTClientNotInittedErr = -3279,
	kOTENETUNREACHErr = -3250, kOTETIMEDOUTErr = -3259, kOTECONNREFUSEDErr = -3260, kOTEHOSTUNREACHErr = -3264
};
enum { kT_CONNECT = 0x0002, kT_DATA = 0x0004, kT_DISCONNECT = 0x0010, kT_ORDREL = 0x0080, kT_GODATA = 0x0100 };
#define kOTDefaultInternetServicesPath (reinterpret_cast<OTConfiguration *>(-3L))
#endif
