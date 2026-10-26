#include "fetch.hpp"
#include "http.hpp"
#include "log.hpp"
#include "version.hpp"
#include <cstring>

namespace wx {

static void dump_text(const char *what, const char *p, size_t n)
{
	if (log_level() < LogDebug) return;
	if (n > 1500) n = 1500;
	char tmp[1600];
	size_t k = 0;
	for (size_t i = 0; i < n && k + 1 < sizeof tmp; i++) {
		unsigned char c = static_cast<unsigned char>(p[i]);
		tmp[k++] = (c == '\r' || c == '\n' || c == '\t' || (c >= 32 && c < 127)) ? static_cast<char>(c) : '.';
	}
	tmp[k] = 0;
	logf(LogDebug, "%s (%zu bytes):\n%s", what, n, tmp);
}

static Status io_fail(const Io &io, const char *what)
{
	Err e = io.err == Err::Ok ? Err::Io : io.err;
	return make(e, io.msg ? io.msg : what, io.detail);
}

// The attempt's stream as the rest of fetch_once and the TLS layer see it: every read and write first checks the attempt's
// deadline. A check only between reads would miss the TLS handshake and each TLS record, which are single blocking calls
// (many raw reads, each within the timeout) underneath. Lives on the stack; no heap.
namespace {
struct DeadlineIo {
	Io io;                       // io.ctx points back at this struct
	Io *inner;
	Net *net;
	long long start, limit;
};
long dl_late(DeadlineIo *d)
{
	d->io.err = Err::Timeout; d->io.detail = 0; d->io.msg = "answer not complete within the time limit";
	return -1;
}
bool dl_expired(const DeadlineIo *d) { return d->net->now && d->net->now(*d->net) - d->start > d->limit; }
long dl_read(Io &io, void *buf, size_t len)
{
	DeadlineIo *d = static_cast<DeadlineIo *>(io.ctx);
	if (dl_expired(d)) return dl_late(d);
	long r = d->inner->read(*d->inner, buf, len);
	if (r < 0) { io.err = d->inner->err; io.detail = d->inner->detail; io.msg = d->inner->msg; }
	return r;
}
long dl_write(Io &io, const void *buf, size_t len)
{
	DeadlineIo *d = static_cast<DeadlineIo *>(io.ctx);
	if (dl_expired(d)) return dl_late(d);
	long r = d->inner->write(*d->inner, buf, len);
	if (r < 0) { io.err = d->inner->err; io.detail = d->inner->detail; io.msg = d->inner->msg; }
	return r;
}
} // namespace

Status fetch_once(Net &net, const char *host, const char *port, bool tls, const char *path,
    const FetchOpts &o, char *buf, size_t cap, sv *body)
{
	if (tls && net.now && net.now(net) < kMinSaneTime)
		return make(Err::Clock, "system clock is before 2025; set the date (needed to check certificates)");

	// The timeout bounds each wait; this bounds the attempt, so a server that trickles one byte at a time cannot hold it for
	// hours. Measured with the platform clock (whole seconds); if the clock steps back the elapsed time is negative, which is safe.
	const long long start = net.now ? net.now(net) : 0;
	const long long limit = o.deadline_s > 0 ? o.deadline_s : 3LL * o.timeout_s;

	Io raw;
	logf(LogDebug, "connecting to %s:%s%s", host, port, tls ? " (tls)" : "");
	Status s = net.connect(net, host, port, o.timeout_s, &raw);
	if (!s.ok()) return s;

	DeadlineIo dio;
	dio.io.ctx = &dio; dio.io.read = dl_read; dio.io.write = dl_write;
	dio.inner = &raw; dio.net = &net; dio.start = start; dio.limit = limit;
	Io tio;
	Io *io = &dio.io;
	if (tls) {
		if (!net.tls_open) { net.close(net, raw); return make(Err::Tls, "no TLS support built in"); }
		s = net.tls_open(net, dio.io, host, &tio);
		if (!s.ok()) { net.close(net, raw); return s; }
		io = &tio;
	}

	Buf<160> agent;
	agent.add("weather/" WEATHER_VERSION);
	if (o.contact && o.contact[0]) agent.add(" (contact: ").add(o.contact).add(")");
	Buf<640> req;
	if (!http_request(req, host, path, agent.view())) {
		s = make(Err::BadData, "request too long");
	} else {
		dump_text("request", req.s, req.n);
		long w = io->write(*io, req.s, req.n);
		if (w < 0) s = io_fail(*io, "sending request failed");
	}

	size_t got = 0;
	while (s.ok()) {
		if (got == cap) { s = make(Err::BadData, "response too large"); break; }
		long r = io->read(*io, buf + got, cap - got);
		if (r < 0) { s = io_fail(*io, "reading response failed"); break; }
		if (r == 0) break;
		got += static_cast<size_t>(r);
	}
	if (tls) net.tls_close(net, tio);
	net.close(net, raw);
	if (!s.ok()) return s;

	dump_text("response", buf, got);
	HttpResponse h = http_parse(sv(buf, got));
	if (!h.st.ok()) return h.st;
	if (h.code != 200) return make(Err::HttpStatus, "server returned an error status", h.code);
	*body = h.body;
	return okay();
}

Status fetch_retry(Net &net, const char *host, const char *port, bool tls, const char *path,
    const FetchOpts &o, char *buf, size_t cap, sv *body)
{
	long delay = o.backoff_ms < o.max_backoff_ms ? o.backoff_ms : o.max_backoff_ms;
	Status s;
	for (int attempt = 1; attempt <= o.attempts; attempt++) {
		s = fetch_once(net, host, port, tls, path, o, buf, cap, body);
		if (s.ok()) return s;
		logf(LogWarn, "%s: attempt %d/%d failed: %s: %s (detail %d)", host, attempt, o.attempts,
		    err_name(s.err), s.msg, s.detail);
		if (!retryable(s) || attempt == o.attempts) break;
		if (net.sleep_ms && !net.sleep_ms(net, delay)) return make(Err::Cancelled, "cancelled while waiting to retry");
		delay = delay > o.max_backoff_ms / 2 ? o.max_backoff_ms : delay * 2;
	}
	return s;
}

Status fetch_forecast(Net &net, const Provider *const *providers, size_t n, const Query &q,
    const FetchOpts &o, char *buf, size_t cap, Forecast *out)
{
	Status last = make(Err::NoSource, "no data sources configured");
	for (size_t i = 0; i < n; i++) {
		const Provider &p = *providers[i];
		char path[320];
		if (!p.path(q, path, sizeof path)) {
			last = make(Err::BadData, "request path too long");
			continue;
		}
		if (p.needs_contact && !(o.contact && o.contact[0])) {
			last = make(Err::Config, "this data source requires 'contact' in the config file (see weatherrc(5))");
			logf(LogWarn, "%s skipped: %s", p.name, last.msg);
			continue;
		}
		const char *host = o.force_host ? o.force_host : p.host;
		const char *port = o.force_port ? o.force_port : p.port;
		bool tls = o.force_host == nullptr;
		logf(LogInfo, "trying %s (%s:%s)", p.name, host, port);
		sv body;
		Status s = fetch_retry(net, host, port, tls, path, o, buf, cap, &body);
		if (s.ok()) {
			Forecast f;
			s = p.parse(body, q, &f);
			if (!s.ok())
				logf(LogWarn, "%s: unusable response: %s: %s", p.name, err_name(s.err), s.msg);
			else { *out = f; return okay(); }
		} else {
			logf(LogWarn, "%s failed: %s: %s (detail %d)", p.name, err_name(s.err), s.msg, s.detail);
		}
		last = s;
		if (s.err == Err::Cancelled) return s;   // do not fall back after a cancel
	}
	if (n > 1) return make(Err::NoSource, "all data sources failed", static_cast<int>(n));
	return last;
}

} // namespace wx
