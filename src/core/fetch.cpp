// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#include "fetch.hpp"
#include "http.hpp"
#include "log.hpp"
#include "version.hpp"
#include <cstring>

namespace wx {

static void dump_text(const char *what, const char *p, size_t n)
{
	if (log_level() < LogDebug) return;
	// Both platform loggers bound a formatted record to 512 bytes. Keep each
	// payload small and on one line, with offsets for reconstructing the dump.
	logf(LogDebug, "%s (%zu bytes):", what, n);
	char chunk[241];
	for (size_t offset = 0; offset < n;) {
		size_t count = n - offset;
		if (count > sizeof chunk - 1) count = sizeof chunk - 1;
		for (size_t i = 0; i < count; i++) {
			unsigned char c = static_cast<unsigned char>(p[offset + i]);
			chunk[i] = c >= 32 && c < 127 ? static_cast<char>(c) : '.';
		}
		chunk[count] = 0;
		logf(LogDebug, "%s [%zu]: %s", what, offset, chunk);
		offset += count;
	}
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
struct BudgetScope {
	Net &net;
	const TimeBudget *previous;
	TimeBudget current;
	BudgetScope(Net &n, long long start, long long limit) : net(n), previous(n.budget), current{start, limit} { net.budget = &current; }
	~BudgetScope() { net.budget = previous; }
	BudgetScope(const BudgetScope &) = delete;
	BudgetScope &operator=(const BudgetScope &) = delete;
};
struct DeadlineIo {
	Io io;                       // io.ctx points back at this struct
	Io *inner;
	Net *net;
	long long start, limit, last;
};
long dl_late(DeadlineIo *d)
{
	d->io.err = Err::Timeout; d->io.detail = 0; d->io.msg = "answer not complete within the time limit";
	return -1;
}
bool dl_expired(DeadlineIo *d)
{
	long long current = d->net->monotonic_ms(*d->net);
	if (current < d->last) {
		d->io.err = Err::Clock; d->io.msg = "monotonic clock unavailable or moved backwards";
		return true;
	}
	d->last = current;
	// Both values are nonnegative and ordered, so subtraction cannot overflow.
	if (current - d->start >= d->limit) { dl_late(d); return true; }
	return false;
}
long dl_read(Io &io, void *buf, size_t len)
{
	DeadlineIo *d = static_cast<DeadlineIo *>(io.ctx);
	if (dl_expired(d)) return -1;
	long r = d->inner->read(*d->inner, buf, len);
	if (r < 0) { io.err = d->inner->err; io.detail = d->inner->detail; io.msg = d->inner->msg; }
	return r;
}
long dl_write(Io &io, const void *buf, size_t len)
{
	DeadlineIo *d = static_cast<DeadlineIo *>(io.ctx);
	if (dl_expired(d)) return -1;
	long r = d->inner->write(*d->inner, buf, len);
	if (r < 0) { io.err = d->inner->err; io.detail = d->inner->detail; io.msg = d->inner->msg; }
	return r;
}
} // namespace

Status fetch_once(Net &net, const char *host, const char *port, bool tls, const char *path,
    const FetchOpts &o, char *buf, size_t cap, sv *body)
{
	if(o.metadata) *o.metadata=FetchMetadata{};
	if (tls && net.now && net.now(net) < kMinSaneTime)
		return make(Err::Clock, "system clock is before 2025; set the date (needed to check certificates)");

	// The timeout bounds each wait; this bounds the attempt, so a server that trickles one byte at a time cannot hold it for
	// hours. UTC is reserved for certificate dates; active milliseconds respect modal UI pauses.
	if (!net.monotonic_ms) return make(Err::Clock, "no monotonic clock for fetch deadline");
	const long long start = net.monotonic_ms(net);
	if (start < 0) return make(Err::Clock, "monotonic clock unavailable");
	if (o.timeout_s <= 0 || o.deadline_s < 0) return make(Err::Config, "invalid fetch timeout");
	long long limit = o.deadline_s > 0 ? 1000LL * o.deadline_s : 3000LL * o.timeout_s;

	if (net.budget) {
		long long remaining = net.budget->remaining(start);
		if (remaining < 0) return make(Err::Clock, "monotonic clock unavailable or moved backwards");
		if (remaining == 0) return make(Err::Timeout, "answer not complete within the time limit");
		if (remaining < limit) limit = remaining;
	}
	BudgetScope budget_scope(net, start, limit);

	Io raw;
	logf(LogDebug, "connecting to %s:%s%s", host, port, tls ? " (tls)" : "");
	Status s = net.connect(net, host, port, o.timeout_s, &raw);
	if (!s.ok()) return s;

	DeadlineIo dio;
	dio.io.ctx = &dio; dio.io.read = dl_read; dio.io.write = dl_write;
	dio.inner = &raw; dio.net = &net; dio.start = start; dio.limit = limit; dio.last = start;
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
	agent.add(" (contact: ").add(o.contact && o.contact[0] ? o.contact : "https://github.com/jackwillis/weather").add(")");
	Buf<640> req;
	if (!http_request(req, host, path, agent.view(),o.if_modified_since)) {
		s = make(Err::BadData, "request too long");
	} else {
		dump_text("request", req.s, req.n);
		long w = io->write(*io, req.s, req.n);
		if (w < 0) s = io_fail(*io, "sending request failed");
	}

	size_t got = 0;
	while (s.ok()) {
		if (got == cap) {
			// One bounded probe distinguishes an exact fit from an oversized response.
			char extra; long r=io->read(*io,&extra,1);
			if(r<0) s=io_fail(*io,"reading response failed");
			else if(r>0) s=make(Err::BadData,"response too large");
			break;
		}
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
	if(o.metadata) {
		o.metadata->code=h.code;
		o.metadata->expires=h.expires;
		for(size_t i=0;i<h.last_modified.size();i++) o.metadata->last_modified[i]=h.last_modified[i];
	}
	if(h.code==304 && !o.if_modified_since.empty() && o.metadata) { *body={}; return okay(); }
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
	bool attempted = false;
	for (size_t i = 0; i < n; i++) {
		const Provider &p = *providers[i];
		if (!o.save_forecasts && sv(p.name)=="met-no") {
			Status skipped=make(Err::Config,"MET Norway requires forecast saving");
			if (!attempted) last=skipped;
			logf(LogWarn,"%s",skipped.msg); continue;
		}
		char path[320];
		if (!p.path(q, path, sizeof path)) {
			if (!attempted) last = make(Err::BadData, "request path too long");
			continue;
		}
		const char *host = o.force_host ? o.force_host : p.host;
		const char *port = o.force_port ? o.force_port : p.port;
		bool tls = o.force_host == nullptr;
		logf(LogInfo, "trying %s (%s:%s)", p.name, host, port);
		FetchOpts request=o; FetchMetadata metadata;
		request.metadata=&metadata; request.if_modified_since={};
		const ConditionalForecast *previous=o.conditional;
		bool matching=sv(p.name)=="met-no" && previous && previous->query.lat_e4==q.lat_e4 &&
		    previous->query.lon_e4==q.lon_e4 && previous->query.metric==q.metric &&
		    previous->forecast.metric==q.metric && previous->forecast.source && sv(previous->forecast.source)=="met-no" &&
		    check_fields(previous->forecast,q.metric).ok() &&
		    plausible_temp_t10(previous->forecast.temp_t10,q.metric);
        if(matching && net.now && previous->forecast.expires>net.now(net)) {
            long long now=net.now(net);
            if(previous->fetched_at>=0 && now-previous->fetched_at>86400) {
                if(!attempted) last=make(Err::NoSource,"saved MET forecast too old; waiting for server expiry");
                continue; // honor expiry without presenting old values as fresh
            }
            *out=previous->forecast; out->saved_valid=true;
            out->retrieved_at=previous->fetched_at>=0 ? previous->fetched_at : now;
            logf(LogInfo,"MET Norway: using saved forecast until server expiry");
            return okay();
        }
		if(matching) {
			size_t length=0;
			while(length<sizeof previous->forecast.last_modified && previous->forecast.last_modified[length]) ++length;
			sv value(previous->forecast.last_modified,length);
			if(!value.empty() && http_validator(value)) request.if_modified_since=value;
		}
		sv body;
		Status s = fetch_retry(net, host, port, tls, path, request, buf, cap, &body);
		if (s.ok()) {
			Forecast f;
			if(metadata.code==304) {
				// fetch_once only accepts 304 after a nonempty matching validator.
				f=previous->forecast; f.saved_valid=false; f.retrieved_at=0;
			} else s = p.parse(body, q, &f);
			if(s.ok() && sv(p.name)=="met-no") f.expires=metadata.expires;
			if(s.ok() && sv(p.name)=="met-no" && metadata.last_modified[0])
				std::memcpy(f.last_modified,metadata.last_modified,sizeof f.last_modified);
			if (!s.ok())
				logf(LogWarn, "%s: unusable response: %s: %s", p.name, err_name(s.err), s.msg);
			else { *out = f; return okay(); }
		} else {
			logf(LogWarn, "%s failed: %s: %s (detail %d)", p.name, err_name(s.err), s.msg, s.detail);
		}
		last = s;
		attempted = true;
		if (s.err == Err::Cancelled) return s;   // do not fall back after a cancel
	}
	return last;
}

} // namespace wx
