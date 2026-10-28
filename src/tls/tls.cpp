// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// BearSSL TLS client over an Io stream. Portable: needs no OS headers beyond getenv.
#include "tls.hpp"
#include "core/log.hpp"
#include <bearssl.h>
#ifdef WEATHER_TEST_ANCHORS
#include "test_anchors.h"   // test/tls_local.sh builds with a throwaway root instead of the real ones (compile-time only)
#else
#include "anchors.h"
#endif
#include "timeconv.hpp"
#include "secret_clear.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace wx {

struct TlsState {
	br_ssl_client_context sc;
	br_x509_minimal_context xc;
	unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];
	br_sslio_context ioc;
	Io *raw;
	bool raw_eof;
};
static TlsState g_tls;

static int raw_read(void *ctx, unsigned char *buf, size_t len)
{
	Io *raw = static_cast<Io *>(ctx);
	long n = raw->read(*raw, buf, len);
	if (n == 0) g_tls.raw_eof = true;
	return n <= 0 ? -1 : static_cast<int>(n);
}

static int raw_write(void *ctx, const unsigned char *buf, size_t len)
{
	Io *raw = static_cast<Io *>(ctx);
	long n = raw->write(*raw, buf, len);
	return n < 0 ? -1 : static_cast<int>(n);
}

static Status map_engine_error(int e, const Io &raw)
{
	if (e == BR_ERR_X509_EXPIRED || e == BR_ERR_X509_TIME_UNKNOWN)
		return make(Err::CertUntrusted, "certificate expired or not yet valid (is the clock right?)", e);
	if (e == BR_ERR_X509_BAD_SERVER_NAME || e == BR_ERR_X509_DN_MISMATCH)
		return make(Err::CertUntrusted, "certificate is for a different host name", e);
	if (e == BR_ERR_X509_NOT_TRUSTED)
		return make(Err::CertUntrusted, "certificate chain does not end at a trusted root", e);
	if (e >= BR_ERR_X509_OK && e <= BR_ERR_X509_WEAK_PUBLIC_KEY)
		return make(Err::CertUntrusted, "certificate rejected", e);
	if (e == BR_ERR_IO && raw.err != Err::Ok)
		return make(raw.err, "network error during TLS", raw.detail);
	if (e == BR_ERR_IO && g_tls.raw_eof)
		return make(Err::Truncated,"TLS connection closed without close_notify");
	if (e == BR_ERR_IO) return make(Err::Io, "connection closed during TLS", 0);
	return make(Err::Tls, "TLS failure", e);
}

static long tls_read(Io &io, void *buf, size_t len)
{
	TlsState *t = static_cast<TlsState *>(io.ctx);
	int n = br_sslio_read(&t->ioc, buf, len);
	if (n > 0) return n;
	int e = br_ssl_engine_last_error(&t->sc.eng);
	// Only an authenticated TLS closure is a clean EOF. An abrupt transport
	// close must remain retryable even for an HTTP close-delimited body.
	if (e == BR_ERR_OK) return 0;
	Status s = map_engine_error(e, *t->raw);
	io.err = s.err; io.detail = s.detail; io.msg = s.msg;
	return -1;
}

static long tls_write(Io &io, const void *buf, size_t len)
{
	TlsState *t = static_cast<TlsState *>(io.ctx);
	if (br_sslio_write_all(&t->ioc, buf, len) < 0 || br_sslio_flush(&t->ioc) < 0) {
		Status s = map_engine_error(br_ssl_engine_last_error(&t->sc.eng), *t->raw);
		io.err = s.err; io.detail = s.detail; io.msg = s.msg;
		return -1;
	}
	return static_cast<long>(len);
}

static Status tls_open(Net &net, Io &raw, const char *host, Io *out)
{
	TlsState *t = &g_tls;
	t->raw = &raw;
	t->raw_eof = false;
	size_t nta = TAs_NUM;
	#ifdef WEATHER_TEST_HOOKS
	if (getenv("WEATHER_NO_TA")) nta = 0;
	#endif
	br_ssl_client_init_full(&t->sc, &t->xc, TAs, nta);
	// Owner policy (#66): TLS 1.2, ephemeral key exchange and authenticated encryption.
	static const uint16_t suites[] = {
		BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
		BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
		BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
		BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
		BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
	};
	br_ssl_engine_set_versions(&t->sc.eng, BR_TLS12, BR_TLS12);
	br_ssl_engine_set_suites(&t->sc.eng, suites, sizeof suites / sizeof suites[0]);
	br_ssl_engine_set_buffer(&t->sc.eng, t->iobuf, sizeof t->iobuf, 1);

	// Certificate validity is judged against the platform clock, not whatever BearSSL guesses.
	if (!net.now) return make(Err::Tls, "no clock available");
	BrTime bt = unix_to_br_time(net.now(net));
	br_x509_minimal_set_time(&t->xc, static_cast<uint32_t>(bt.days), static_cast<uint32_t>(bt.secs));

	// Seed the random generator from the platform. Never run TLS with a missing source.
	unsigned char seed[32] = {};
	if (!net.entropy || !net.entropy(net, seed, sizeof seed)) {
		secret_clear(seed, sizeof seed);
		return make(Err::Tls, "no source of random numbers");
	}
	br_ssl_engine_inject_entropy(&t->sc.eng, seed, sizeof seed);
	secret_clear(seed, sizeof seed);

	if (!br_ssl_client_reset(&t->sc, host, 0))
		return map_engine_error(br_ssl_engine_last_error(&t->sc.eng), raw);
	br_sslio_init(&t->ioc, &t->sc.eng, raw_read, &raw, raw_write, &raw);
	out->ctx = t;
	out->read = tls_read;
	out->write = tls_write;
	out->err = Err::Ok;
	out->detail = 0;
	out->msg = nullptr;
	logf(LogDebug, "tls: client ready for %s (%zu trust anchors)", host, nta);
	return okay();
}

static void tls_close(Net &, Io &io)
{
	TlsState *t = static_cast<TlsState *>(io.ctx);
	if (!t) return;
	br_sslio_close(&t->ioc);
	br_ssl_session_parameters sp;
	br_ssl_engine_get_session_parameters(&t->sc.eng, &sp);
	logf(LogDebug, "tls: closed (version 0x%04x, cipher 0x%04x)", sp.version, sp.cipher_suite);
}

void tls_net_init(Net *net)
{
	net->tls_open = tls_open;
	net->tls_close = tls_close;
}

} // namespace wx
