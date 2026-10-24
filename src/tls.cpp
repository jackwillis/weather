// BearSSL TLS client over an Io stream. Portable: needs no OS headers beyond getenv.
#include "tls.hpp"
#include "log.hpp"
#include <bearssl.h>
#include "anchors.h"
#include <cstdlib>

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
	if (e == BR_ERR_IO) return make(Err::Io, "connection closed during TLS", 0);
	return make(Err::Tls, "TLS failure", e);
}

static long tls_read(Io &io, void *buf, size_t len)
{
	TlsState *t = static_cast<TlsState *>(io.ctx);
	int n = br_sslio_read(&t->ioc, buf, len);
	if (n > 0) return n;
	int e = br_ssl_engine_last_error(&t->sc.eng);
	// Peer ended the stream (with or without close_notify): report EOF. The HTTP
	// layer detects a short body through Content-Length.
	if (e == BR_ERR_OK || (e == BR_ERR_IO && t->raw_eof && t->raw->err == Err::Ok)) return 0;
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

static Status tls_open(Net &, Io &raw, const char *host, Io *out)
{
	TlsState *t = &g_tls;
	t->raw = &raw;
	t->raw_eof = false;
	size_t nta = getenv("WEATHER_NO_TA") ? 0 : TAs_NUM;
	br_ssl_client_init_full(&t->sc, &t->xc, TAs, nta);
	br_ssl_engine_set_buffer(&t->sc.eng, t->iobuf, sizeof t->iobuf, 1);
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
