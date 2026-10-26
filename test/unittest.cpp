// Unit tests: plain C++, no framework. Build-time checks (static_assert) come first:
// if the core is wrong, this file does not compile.
#include "core/config.hpp"
#include "core/fetch.hpp"
#include "core/http.hpp"
#include "core/json.hpp"
#include "core/log.hpp"
#include "core/openmeteo.hpp"
#include "core/version.hpp"
#include "app/locform.hpp"
#include "app/model.hpp"
#include "app/textedit.hpp"
#include "core/providers.hpp"
#include "core/settings.hpp"
#include "platform/posix/net_posix.hpp"
#include "platform/posix/settings_posix.hpp"
#include "tls/timeconv.hpp"
#include "platform/os9/dns_resolve.hpp"
#include "platform/os9/modal_clock.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace wx;

#include "static_tests.hpp"

// ---- runtime tests
static int g_fail = 0, g_run = 0;
#define CHECK(c) do { g_run++; if (!(c)) { g_fail++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#include "seed_tests.hpp"
#include "os9_helper_tests.hpp"

static char g_fixbuf[2][8192];
static const char *fixture(int slot, const char *name)
{
	char path[256];
	snprintf(path, sizeof path, "%s/%s", FIXTURE_DIR, name);
	FILE *f = fopen(path, "rb");
	if (!f) { printf("cannot open %s\n", path); g_fail++; return ""; }
	size_t n = fread(g_fixbuf[slot], 1, sizeof g_fixbuf[slot] - 1, f);
	g_fixbuf[slot][n] = 0;
	fclose(f);
	return g_fixbuf[slot];
}

static void test_openmeteo()
{
	Query q;
	Forecast f;
	Status s = openmeteo_parse(fixture(0, "openmeteo-ok.json"), q, &f);
	CHECK(s.ok());
	CHECK(f.temp_t10 == 548 && f.humidity == 84 && f.wind_t10 == 87 && f.wind_dir == 138);
	CHECK(f.code == 0 && f.is_day == 0 && !strcmp(f.time, "2026-10-24T04:45"));

	Buf<300> full;
	format_full(full, f, "Madison, WI");
	constexpr sv head = "Weather for Madison, WI from open-meteo, observed";
	CHECK(full.view().substr(0, head.size()) == head);

	Buf<300> one;
	format_oneline(one, f);
	CHECK(one.view() == sv("54.8F Clear sky, wind 8.7mph SE, humidity 84% (open-meteo 2026-10-24T04:45)"));

	s = openmeteo_parse(fixture(0, "openmeteo-400.json"), q, &f);
	CHECK(s.err == Err::BadData);
	s = openmeteo_parse(R"({"current":{"temperature_2m":"54.8","weather_code":0}})", q, &f);
	CHECK(s.err == Err::BadData);               // string where a number belongs
	s = openmeteo_parse(R"({"current":{"weather_code":0}})", q, &f);
	CHECK(s.err == Err::BadData);               // missing temperature
	s = openmeteo_parse(R"({"current":{"temperature_2m":54.8)", q, &f);
	CHECK(s.err == Err::Parse);                 // truncated JSON
	s = openmeteo_parse(R"({"current":{"temperature_2m":9999,"weather_code":0}})", q, &f);
	CHECK(s.err == Err::BadData);               // implausible
	s = openmeteo_parse(R"({"current":{"temperature_2m":20.0,"weather_code":150}})", q, &f);
	CHECK(s.err == Err::BadData);               // no such WMO code
	s = openmeteo_parse(R"({"current":{"temperature_2m":20.0,"weather_code":99}})", q, &f);
	CHECK(s.ok());
	{ Query mq; mq.metric = true;               // the real (Fahrenheit, mp/h) fixture answers a metric request: refused, not relabelled
	  Forecast mf;
	  s = openmeteo_parse(fixture(0, "openmeteo-ok.json"), mq, &mf);
	  CHECK(s.err == Err::BadData && strstr(s.msg, "units") != nullptr); }
	s = openmeteo_parse("<html>", q, &f);
	CHECK(s.err == Err::Parse);
	s = openmeteo_parse(R"({"latitude":1})", q, &f);
	CHECK(s.err == Err::BadData);               // no current object

	char path[320];
	q.lat_e4 = 418800; q.lon_e4 = -876300;
	CHECK(openmeteo_path(q, path, sizeof path));
	CHECK(strstr(path, "latitude=41.8800&longitude=-87.6300") != nullptr);
	CHECK(strstr(path, "fahrenheit") != nullptr);
	char tiny[10];
	CHECK(!openmeteo_path(q, tiny, sizeof tiny));
}

static void test_config()
{
	Config c;
	Status s = config_parse("# comment\nlatitude = 51.5074  # london\nlongitude=-0.1278\nunits = metric\n"
	    "timeout=5\nproviders = open-meteo\nlog = info\n", &c);
	CHECK(s.ok() && c.lat_e4 == 515074 && c.lon_e4 == -1278 && c.metric && c.timeout_s == 5 && c.level == LogInfo);
	s = config_parse("units = metric\nbogus = 1\n", &c);
	CHECK(s.err == Err::Config && s.detail == 2);           // line number reported
	s = config_parse("latitude = 91\n", &c);
	CHECK(s.err == Err::Config && s.detail == 1);
	s = config_parse("no equals sign\n", &c);
	CHECK(s.err == Err::Config);
	s = config_parse("name = Oslo, Norway\n", &c);
	CHECK(s.ok() && !strcmp(c.name, "Oslo, Norway"));
	CHECK(config_set(&c, "name", "a#b", 1).err == Err::Config);   // '#' could never be read back
	s = config_parse("name = abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n", &c);
	CHECK(s.err == Err::Config);                            // too long
	CHECK(config_parse("", &c).ok());
	{ Config d; CHECK(d.lat_e4 == 430747 && d.lon_e4 == -893841 && !strcmp(d.name, "Madison, WI")); }
	CHECK(config_parse("units = metric", &c).ok());         // no trailing newline
	{ // classic Mac files end lines with CR alone: a CR is a line break, not whitespace inside one line
		Config d;
		s = config_parse("# my weather\rlatitude = 59.9100\rlongitude = 10.7500\rname = Oslo\r", &d);
		CHECK(s.ok() && d.lat_e4 == 599100 && d.lon_e4 == 107500 && !strcmp(d.name, "Oslo"));
		s = config_parse("units = metric\rbogus = 1\r", &d);
		CHECK(s.err == Err::Config && s.detail == 2);       // the line number counts CR lines
		s = config_parse("# a\r\nlatitude = 1.5\rlongitude = 2.5\nname = X", &d);   // CRLF, CR and LF mixed; no final terminator
		CHECK(s.ok() && d.lat_e4 == 15000 && d.lon_e4 == 25000 && !strcmp(d.name, "X"));
		s = config_parse("units = metric\r\nbogus = 1\r\n", &d);
		CHECK(s.err == Err::Config && s.detail == 2);       // CRLF is one break, not two: still line 2
	}
}

// ---- scripted fake network
struct Step {
	Status connect_result;   // if not ok, connect fails
	const char *response;    // bytes to serve
	Err read_err;            // after serving all bytes: Err::Ok = clean EOF
};
struct Fake {
	Step steps[8];
	int nsteps = 0, next = 0, connects = 0, closes = 0, nsleeps = 0;
	long sleeps[64] = {};
	long long clock = 1790000000LL;
	os9::ModalClock active_clock;
	uint32_t ticks = 0xfffffff0U;
	bool use_active = false, pause_reads = false;
	long long monotonic = 0;
	long long seen_budget_limit = -1;
	long mono_read_ms = 0;
	long read_secs = 0;      // the clock advances this much on every read (a slow trickle)
	int reads = 0;
	bool cancel_sleep = false;
	const char *cur = ""; size_t pos = 0; Err cur_err = Err::Ok;
	char request[512] = {};
};
static Fake *F(Net &n) { return static_cast<Fake *>(n.ctx); }

static long fk_read(Io &io, void *buf, size_t len)
{
	Fake *f = static_cast<Fake *>(io.ctx);
	f->reads++; f->clock += f->read_secs; f->monotonic += f->mono_read_ms;
	uint32_t before = f->ticks; f->ticks += 120;
	if (f->pause_reads) f->active_clock.credit(before, f->ticks);
	size_t left = strlen(f->cur) - f->pos;
	if (left == 0) {
		if (f->cur_err != Err::Ok) { io.err = f->cur_err; io.detail = 1; return -1; }
		return 0;
	}
	size_t n = left < 7 ? left : 7;                  // dribble to test reassembly
	if (n > len) n = len;
	memcpy(buf, f->cur + f->pos, n);
	f->pos += n;
	return static_cast<long>(n);
}
static long fk_write(Io &io, const void *buf, size_t len)
{
	Fake *f = static_cast<Fake *>(io.ctx);
	size_t have = strlen(f->request);
	if (have + len < sizeof f->request) { memcpy(f->request + have, buf, len); f->request[have + len] = 0; }
	return static_cast<long>(len);
}
static Status fk_connect(Net &n, const char *, const char *, int, Io *out)
{
	Fake *f = F(n);
	f->seen_budget_limit = n.budget ? n.budget->limit_ms : -1;
	f->connects++;
	Step &s = f->steps[f->next < f->nsteps ? f->next : f->nsteps - 1];
	f->next++;
	if (!s.connect_result.ok()) return s.connect_result;
	f->cur = s.response; f->pos = 0; f->cur_err = s.read_err;
	f->request[0] = 0;
	out->ctx = f; out->read = fk_read; out->write = fk_write;
	return okay();
}
static void fk_close(Net &n, Io &) { F(n)->closes++; }
static bool fk_sleep(Net &n, long ms) { Fake *f = F(n); if (f->nsleeps < 64) f->sleeps[f->nsleeps++] = ms; return !f->cancel_sleep; }
static long long fk_now(Net &n) { return F(n)->clock; }

static long long fk_monotonic(Net &n) { Fake *f = F(n); return f->use_active ? f->active_clock.milliseconds(f->ticks) : f->monotonic; }

static Net make_net(Fake *f)
{
	Net n;
	n.ctx = f; n.connect = fk_connect; n.close = fk_close; n.sleep_ms = fk_sleep; n.now = fk_now; n.monotonic_ms = fk_monotonic;
	return n;
}

#define RESP_OK(body) "HTTP/1.0 200 OK\r\nContent-Length: " #body "\r\n\r\n"
static const char *kGood = "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n\r\n"
    R"({"current":{"time":"2026-10-24T04:45","temperature_2m":54.8,"weather_code":3}})";
static const char *k500 = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
static const char *k404 = "HTTP/1.0 404 Not Found\r\n\r\n{}";
static const char *kChunked = "HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\n{}\r\n0\r\n\r\n";
static const char *kShort = "HTTP/1.0 200 OK\r\nContent-Length: 500\r\n\r\n{\"cur";

static void test_fetch()
{
	char buf[4096];
	sv body;
	FetchOpts o; o.attempts = 3; o.backoff_ms = 100; o.timeout_s = 1;

	{ // success, request is well formed, connection closed
		Fake f; f.steps[0] = {okay(), kGood, Err::Ok}; f.nsteps = 1;
		Net n = make_net(&f);
		Status s = fetch_once(n, "h.example", "80", false, "/x?y=1", o, buf, sizeof buf, &body);
		CHECK(s.ok() && body.size() > 10);
		CHECK(strstr(f.request, "GET /x?y=1 HTTP/1.0\r\nHost: h.example\r\n") == f.request);
		CHECK(strstr(f.request, "Connection: close") != nullptr);
		CHECK(f.connects == 1 && f.closes == 1);
	}
	{ // 500 is retried with doubling backoff, then reported with its status
		Fake f; f.steps[0] = {okay(), k500, Err::Ok}; f.nsteps = 1;
		Net n = make_net(&f);
		Status s = fetch_retry(n, "h", "80", false, "/", o, buf, sizeof buf, &body);
		CHECK(s.err == Err::HttpStatus && s.detail == 500);
		CHECK(f.connects == 3 && f.closes == 3 && f.nsleeps == 2 && f.sleeps[0] == 100 && f.sleeps[1] == 200);
	}
	{ // 404 is permanent: one try, no sleep
		Fake f; f.steps[0] = {okay(), k404, Err::Ok}; f.nsteps = 1;
		Net n = make_net(&f);
		Status s = fetch_retry(n, "h", "80", false, "/", o, buf, sizeof buf, &body);
		CHECK(s.err == Err::HttpStatus && s.detail == 404 && f.connects == 1 && f.nsleeps == 0);
	}
	{ // transient failure then success
		Fake f;
		f.steps[0] = {make(Err::Timeout, "connect timed out"), "", Err::Ok};
		f.steps[1] = {okay(), kGood, Err::Ok};
		f.nsteps = 2;
		Net n = make_net(&f);
		Status s = fetch_retry(n, "h", "80", false, "/", o, buf, sizeof buf, &body);
		CHECK(s.ok() && f.connects == 2 && f.nsleeps == 1);
	}
	{ // a server that trickles its answer: every read is quick, but the whole attempt must not outlast 3 x the timeout
		Fake f; f.steps[0] = {okay(), kGood, Err::Ok}; f.nsteps = 1; f.mono_read_ms = 1000;     // one second per read, timeout_s = 1
		Net n = make_net(&f);
		Status s = fetch_once(n, "h", "80", false, "/", o, buf, sizeof buf, &body);
		CHECK(s.err == Err::Timeout && f.closes == 1 && f.reads == 4);                     // reads at 0, 1, 2 and 3 s elapsed; the next is refused (limit 3 s)
		Fake g; g.steps[0] = {okay(), kGood, Err::Ok}; g.nsteps = 1;                      // the same answer at full speed is fine
		Net gn = make_net(&g);
		CHECK(fetch_once(gn, "h", "80", false, "/", o, buf, sizeof buf, &body).ok());
		Fake r; r.steps[0] = {okay(), kGood, Err::Ok}; r.nsteps = 1; r.mono_read_ms = 1000;
		Net rn = make_net(&r);
		Status rs = fetch_retry(rn, "h", "80", false, "/", o, buf, sizeof buf, &body);    // a timeout is retried, each attempt bounded
		CHECK(rs.err == Err::Timeout && r.connects == 3 && r.closes == 3);
		Fake e; e.steps[0] = {okay(), kGood, Err::Ok}; e.nsteps = 1; e.mono_read_ms = 1000;
		Net en = make_net(&e);
		Fake t; t.steps[0] = {okay(), kGood, Err::Ok}; t.nsteps = 1; t.mono_read_ms = 1000;
		Net tn = make_net(&t);
		FetchOpts tight = o; tight.deadline_s = 2;                                            // a deadline below 3 x timeout wins
		CHECK(fetch_once(tn, "h", "80", false, "/", tight, buf, sizeof buf, &body).err == Err::Timeout && t.reads == 3);
		FetchOpts roomy = o; roomy.deadline_s = 100;                                         // an explicit deadline replaces 3 x timeout
		CHECK(fetch_once(en, "h", "80", false, "/", roomy, buf, sizeof buf, &body).ok());
		Fake b; b.steps[0] = {okay(), kGood, Err::Ok}; b.nsteps = 1; b.read_secs = -100;     // the clock steps back mid-fetch: never a false timeout
		Net bn = make_net(&b);
		CHECK(fetch_once(bn, "h", "80", false, "/", o, buf, sizeof buf, &body).ok());
	}
	{ // The attempt budget reaches backend waits, and restores caller state.
		Fake f; f.steps[0] = {okay(), kGood, Err::Ok}; f.nsteps = 1;
		Net n = make_net(&f); FetchOpts explicit_limit = o; explicit_limit.deadline_s = 1;
		CHECK(fetch_once(n, "h", "80", false, "/", explicit_limit, buf, sizeof buf, &body).ok());
		CHECK(f.seen_budget_limit == 1000 && n.budget == nullptr && n.ctx == &f);
		f.monotonic = 200;
		const TimeBudget outer{0, 500}; n.budget = &outer;
		CHECK(fetch_once(n, "h", "80", false, "/", explicit_limit, buf, sizeof buf, &body).ok());
		CHECK(f.seen_budget_limit == 300 && n.budget == &outer);
		f.steps[0].connect_result = make(Err::Dns, "failed");
		CHECK(fetch_once(n, "h", "80", false, "/", explicit_limit, buf, sizeof buf, &body).err == Err::Dns);
		CHECK(n.budget == &outer);
		CHECK((TimeBudget{0, 0}.remaining(0) == 0));
		CHECK((TimeBudget{100, 10}.remaining(99) == -1));
		CHECK((TimeBudget{9223372036854775000LL, 500}.remaining(9223372036854775100LL) == 400));
	}
	{ // UTC corrections and paused active time cannot change the attempt deadline.
		for (long step : {-100L, 100L}) {
			Fake f; f.steps[0] = {okay(), kGood, Err::Ok}; f.nsteps = 1;
			f.read_secs = step; // an arbitrarily long menu hold also advances UTC only
			Net n = make_net(&f);
			CHECK(fetch_once(n, "h", "80", false, "/", o, buf, sizeof buf, &body).ok());
			f.mono_read_ms = 1000;
			CHECK(fetch_once(n, "h", "80", false, "/", o, buf, sizeof buf, &body).err == Err::Timeout);
		}
		Fake f; f.steps[0] = {okay(), kGood, Err::Ok}; f.nsteps = 1;
		Net n = make_net(&f); n.monotonic_ms = nullptr;
		CHECK(fetch_once(n, "h", "80", false, "/", o, buf, sizeof buf, &body).err == Err::Clock && f.connects == 0);
		n.monotonic_ms = fk_monotonic; f.monotonic = -1;
		CHECK(fetch_once(n, "h", "80", false, "/", o, buf, sizeof buf, &body).err == Err::Clock && f.connects == 0);
		f.monotonic = 9223372036854775000LL; f.mono_read_ms = -1000;
		CHECK(fetch_once(n, "h", "80", false, "/", o, buf, sizeof buf, &body).err == Err::Clock && f.closes == 1);
	}
	{ // The actual OS9 active clock crosses TickCount wrap, credits menu holds, and resumes deadlines.
		Fake f; f.steps[0] = {okay(), kGood, Err::Ok}; f.nsteps = 1;
		f.use_active = true; f.pause_reads = true;
		Net n = make_net(&f);
		CHECK(fetch_once(n, "h", "80", false, "/", o, buf, sizeof buf, &body).ok());
		f.pause_reads = false;
		CHECK(fetch_once(n, "h", "80", false, "/", o, buf, sizeof buf, &body).err == Err::Timeout);
	}
	{ // TLS handshake raw I/O is subject to the same deadline and preserves the error.
		Fake f; f.steps[0] = {okay(), kGood, Err::Ok}; f.nsteps = 1; f.mono_read_ms = 1000;
		Net n = make_net(&f);
		n.tls_open = [](Net &, Io &raw, const char *, Io *) -> Status {
			char b[1]; while (raw.read(raw, b, sizeof b) > 0) {}
			return raw.err == Err::Ok ? make(Err::Tls, "test handshake unexpectedly reached EOF") : make(raw.err, raw.msg);
		};
		CHECK(fetch_once(n, "h", "443", true, "/", o, buf, sizeof buf, &body).err == Err::Timeout && f.closes == 1);
	}
	{ // backoff doubles but is capped (default 30 s): a long run of attempts never sleeps for hours or overflows a 32-bit long
		Fake f; f.steps[0] = {okay(), k500, Err::Ok}; f.nsteps = 1;
		Net n = make_net(&f);
		FetchOpts many = o; many.attempts = 40; many.backoff_ms = 100;
		Status s = fetch_retry(n, "h", "80", false, "/", many, buf, sizeof buf, &body);
		CHECK(s.err == Err::HttpStatus && f.connects == 40 && f.nsleeps == 39);
		bool in_range = true;
		for (int i = 0; i < f.nsleeps; i++) if (f.sleeps[i] <= 0 || f.sleeps[i] > 30000) in_range = false;
		CHECK(in_range);
		CHECK(f.sleeps[0] == 100 && f.sleeps[8] == 25600 && f.sleeps[9] == 30000 && f.sleeps[38] == 30000);
		Fake g; g.steps[0] = {okay(), k500, Err::Ok}; g.nsteps = 1;
		Net gn = make_net(&g);
		FetchOpts tight = o; tight.attempts = 4; tight.backoff_ms = 5000; tight.max_backoff_ms = 1000;   // a first delay above the cap is capped too
		fetch_retry(gn, "h", "80", false, "/", tight, buf, sizeof buf, &body);
		CHECK(g.nsleeps == 3 && g.sleeps[0] == 1000 && g.sleeps[1] == 1000 && g.sleeps[2] == 1000);
	}
	{ // chunked response: refused, not retried
		Fake f; f.steps[0] = {okay(), kChunked, Err::Ok}; f.nsteps = 1;
		Net n = make_net(&f);
		Status s = fetch_retry(n, "h", "80", false, "/", o, buf, sizeof buf, &body);
		CHECK(s.err == Err::BadData && f.connects == 1);
	}
	{ // connection dies mid-body: truncated, retried
		Fake f; f.steps[0] = {okay(), kShort, Err::Ok}; f.nsteps = 1;
		Net n = make_net(&f);
		Status s = fetch_retry(n, "h", "80", false, "/", o, buf, sizeof buf, &body);
		CHECK(s.err == Err::Truncated && f.connects == 3 && f.closes == 3);
	}
	{ // connection closed before the status line is complete (hangup, or cut early): truncated, retried
		const char *cuts[] = {"", "HTTP/", "HTTP/1.1 20"};
		for (const char *cut : cuts) {
			Fake f; f.steps[0] = {okay(), cut, Err::Ok}; f.nsteps = 1;
			Net n = make_net(&f);
			Status s = fetch_retry(n, "h", "80", false, "/", o, buf, sizeof buf, &body);
			CHECK(s.err == Err::Truncated && f.connects == 3 && f.closes == 3);
		}
	}
	{ // read error mid-response is an Io/Timeout error and the socket is still closed
		Fake f; f.steps[0] = {okay(), "HTTP/1.0 200 OK\r\n", Err::Timeout}; f.nsteps = 1;
		Net n = make_net(&f);
		Status s = fetch_once(n, "h", "80", false, "/", o, buf, sizeof buf, &body);
		CHECK(s.err == Err::Timeout && f.closes == 1);
	}
	{ // wrong clock: refuse before touching the network
		Fake f; f.steps[0] = {okay(), kGood, Err::Ok}; f.nsteps = 1; f.clock = 100000;
		Net n = make_net(&f);
		Status s = fetch_once(n, "h", "443", true, "/", o, buf, sizeof buf, &body);
		CHECK(s.err == Err::Clock && f.connects == 0);
	}
	{ // TLS requested but not built in
		Fake f; f.steps[0] = {okay(), kGood, Err::Ok}; f.nsteps = 1;
		Net n = make_net(&f);
		Status s = fetch_once(n, "h", "443", true, "/", o, buf, sizeof buf, &body);
		CHECK(s.err == Err::Tls && f.closes == 1);
	}
	{ // response bigger than the buffer
		static char big[3000];
		memset(big, 'x', sizeof big - 1);
		memcpy(big, "HTTP/1.0 200 OK\r\n\r\n", 19);
		Fake f; f.steps[0] = {okay(), big, Err::Ok}; f.nsteps = 1;
		Net n = make_net(&f);
		char small[512];
		Status s = fetch_once(n, "h", "80", false, "/", o, small, sizeof small, &body);
		CHECK(s.err == Err::BadData && f.closes == 1);
	}
}

static void test_fallback()
{
	char buf[4096];
	FetchOpts o; o.attempts = 1; o.backoff_ms = 1;
	o.force_host = "fake"; o.force_port = "80";   // plain HTTP: the fake network has no TLS
	Provider second = kOpenMeteo;
	second.name = "second";
	const Provider *two[] = { &kOpenMeteo, &second };
	Query q;
	Forecast out;
	{ // first source down, second answers
		Fake f;
		f.steps[0] = {make(Err::Dns, "no such host"), "", Err::Ok};
		f.steps[1] = {okay(), kGood, Err::Ok};
		f.nsteps = 2;
		Net n = make_net(&f);
		Status s = fetch_forecast(n, two, 2, q, o, buf, sizeof buf, &out);
		CHECK(s.ok() && out.temp_t10 == 548 && f.connects == 2);
	}
	{ // first source returns junk, second answers
		Fake f;
		f.steps[0] = {okay(), "HTTP/1.0 200 OK\r\n\r\n<html>", Err::Ok};
		f.steps[1] = {okay(), kGood, Err::Ok};
		f.nsteps = 2;
		Net n = make_net(&f);
		CHECK(fetch_forecast(n, two, 2, q, o, buf, sizeof buf, &out).ok());
	}
	{ // everything fails
		Fake f; f.steps[0] = {make(Err::Connect, "refused"), "", Err::Ok}; f.nsteps = 1;
		Net n = make_net(&f);
		Status s = fetch_forecast(n, two, 2, q, o, buf, sizeof buf, &out);
		CHECK(s.err == Err::NoSource && s.detail == 2);
	}
	{ // single source: the real error comes through (so the exit code is meaningful)
		Fake f; f.steps[0] = {make(Err::Connect, "refused"), "", Err::Ok}; f.nsteps = 1;
		Net n = make_net(&f);
		Status s = fetch_forecast(n, two, 1, q, o, buf, sizeof buf, &out);
		CHECK(s.err == Err::Connect);
	}
	CHECK(exit_code(Err::Dns) == 68 && exit_code(Err::Timeout) == 75 && exit_code(Err::Usage) == 64);
	CHECK(retryable(make(Err::HttpStatus, "", 503)) && !retryable(make(Err::HttpStatus, "", 404)));
	CHECK(!retryable(make(Err::CertUntrusted, "")) && !retryable(make(Err::Clock, "")));
}


static void test_cancel()
{
	char buf[4096];
	sv body;
	FetchOpts o; o.attempts = 3; o.backoff_ms = 100; o.timeout_s = 1;
	{ // a read that reports Cancelled is final: no retry, no sleep, socket closed
		Fake f; f.steps[0] = {okay(), "HTTP/1.0 200 OK\r\n", Err::Cancelled}; f.nsteps = 1;
		Net n = make_net(&f);
		Status s = fetch_retry(n, "h", "80", false, "/", o, buf, sizeof buf, &body);
		CHECK(s.err == Err::Cancelled && f.connects == 1 && f.closes == 1 && f.nsleeps == 0);
	}
	{ // cancelled while backing off between attempts
		Fake f; f.steps[0] = {okay(), k500, Err::Ok}; f.nsteps = 1; f.cancel_sleep = true;
		Net n = make_net(&f);
		Status s = fetch_retry(n, "h", "80", false, "/", o, buf, sizeof buf, &body);
		CHECK(s.err == Err::Cancelled && f.connects == 1 && f.nsleeps == 1);
	}
	{ // a cancel does not fall through to the next data source
		Provider second = kOpenMeteo; second.name = "second";
		const Provider *two[] = { &kOpenMeteo, &second };
		FetchOpts po; po.attempts = 1; po.force_host = "fake"; po.force_port = "80";
		Fake f; f.steps[0] = {make(Err::Cancelled, "cancelled"), "", Err::Ok}; f.nsteps = 1;
		Net n = make_net(&f);
		Forecast out; Query q;
		Status s = fetch_forecast(n, two, 2, q, po, buf, sizeof buf, &out);
		CHECK(s.err == Err::Cancelled && f.connects == 1);
	}
	CHECK(exit_code(Err::Cancelled) == 130 && !retryable(make(Err::Cancelled, "")));
}

// ---- the real POSIX network layer, against a local socket that never answers
static int listen_local(int *port)
{
	int ls = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in a;
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (ls < 0 || bind(ls, reinterpret_cast<struct sockaddr *>(&a), sizeof a) != 0 || listen(ls, 4) != 0) return -1;
	socklen_t len = sizeof a;
	getsockname(ls, reinterpret_cast<struct sockaddr *>(&a), &len);
	*port = ntohs(a.sin_port);
	return ls;
}
static long long now_ms()
{
	struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
	return static_cast<long long>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}
static int g_yields, g_cancel_at;
static bool count_yield(Net &) { g_yields++; return !(g_cancel_at && g_yields >= g_cancel_at); }

static void test_posix_net()
{
	int port = 0;
	int ls = listen_local(&port);
	CHECK(ls >= 0);
	if (ls < 0) return;
	char portstr[16];
	snprintf(portstr, sizeof portstr, "%d", port);
	char buf[64];
	{ // yield can cancel a blocked read quickly
		Net n; posix_net_init(&n);
		n.yield = count_yield; g_yields = 0; g_cancel_at = 0;
		Io io;
		Status s = n.connect(n, "127.0.0.1", portstr, 10, &io);
		CHECK(s.ok());
		if (!s.ok()) { close(ls); return; }
		g_yields = 0; g_cancel_at = 3; // count only the blocked read, not connection setup
		long long t0 = now_ms();
		long r = io.read(io, buf, sizeof buf);
		long long took = now_ms() - t0;
		CHECK(r < 0 && io.err == Err::Cancelled);
		CHECK(g_yields == 3 && took >= 90 && took < 1000);    // ~50 ms slices
		n.close(n, io);
	}
	{ // without a yield hook a silent server ends in Timeout, after about the timeout
		Net n; posix_net_init(&n);
		Io io;
		CHECK(n.connect(n, "127.0.0.1", portstr, 1, &io).ok());
		long long t0 = now_ms();
		long r = io.read(io, buf, sizeof buf);
		long long took = now_ms() - t0;
		CHECK(r < 0 && io.err == Err::Timeout && took >= 900 && took < 3000);
		n.close(n, io);
	}
	{ // yield keeps running while we wait, and a yield that says yes does not disturb the read
		Net n; posix_net_init(&n);
		n.yield = count_yield; g_yields = 0; g_cancel_at = 0;
		Io io;
		CHECK(n.connect(n, "127.0.0.1", portstr, 1, &io).ok());
		long r = io.read(io, buf, sizeof buf);
		CHECK(r < 0 && io.err == Err::Timeout && g_yields >= 10);
		n.close(n, io);
	}
	{ // A shorter whole-attempt budget reaches the real silent socket wait.
		int silent_port = 0;
		int silent = listen_local(&silent_port);
		CHECK(silent >= 0);
		if (silent >= 0) {
			char service[16], response[4096];
			snprintf(service, sizeof service, "%d", silent_port);
			Net n; posix_net_init(&n);
			FetchOpts opts; opts.timeout_s = 10; opts.deadline_s = 1;
			sv body;
			long long began = now_ms();
			Status status = fetch_once(n, "127.0.0.1", service, false, "/", opts, response, sizeof response, &body);
			long long elapsed = now_ms() - began;
			CHECK(status.err == Err::Timeout && elapsed >= 900 && elapsed < 3000);
			CHECK(n.budget == nullptr);
			close(silent);
		}
	}
	{ // sleep: completes, or stops when yield cancels
		Net n; posix_net_init(&n);
		long long t0 = now_ms();
		CHECK(n.sleep_ms(n, 120));
		CHECK(now_ms() - t0 >= 110);
		n.yield = count_yield; g_yields = 0; g_cancel_at = 1;
		t0 = now_ms();
		CHECK(!n.sleep_ms(n, 5000));
		CHECK(now_ms() - t0 < 500);
	}
	{ // connection refused after the listener is gone
		close(ls);
		Net n; posix_net_init(&n);
		Io io;
		Status s = n.connect(n, "127.0.0.1", portstr, 2, &io);
		CHECK(s.err == Err::Connect);
	}
	{ // entropy: fills the buffer, and two draws differ
		Net n; posix_net_init(&n);
		unsigned char a[16] = {}, b[16] = {};
		CHECK(n.entropy(n, a, sizeof a) && n.entropy(n, b, sizeof b));
		CHECK(memcmp(a, b, sizeof a) != 0);
		unsigned char zero[16] = {};
		CHECK(memcmp(a, zero, sizeof a) != 0);
	}
	{ // clock: sane, and WEATHER_NOW overrides it for tests
		Net n; posix_net_init(&n);
		unsetenv("WEATHER_NOW");
		CHECK(n.now(n) > kMinSaneTime);
		setenv("WEATHER_NOW", "4102444800", 1);
		CHECK(n.now(n) == 4102444800LL);
		unsetenv("WEATHER_NOW");
	}
	CHECK(unix_to_br_time(1790000000LL).days == 719528 + 1790000000LL / 86400);
}

// ---- settings: text editing
static bool upd(const char *in, const KeyVal *kv, size_t n, char *out, size_t cap, Status *st = nullptr)
{
	size_t on = 0;
	Status s = settings_update(in, kv, n, out, cap, &on);
	if (st) *st = s;
	return s.ok();
}

static void test_settings_text()
{
	char out[1024];
	const KeyVal loc[] = { {"latitude", "59.9100"}, {"longitude", "10.7500"}, {"name", "Oslo"} };
	{ // comments, blanks, order and unrelated settings survive; inline comment kept; absent keys appended
		const char *in = "# my weather setup\nlatitude = 1.0000   # home\n\nunits = metric\nlog = info\n";
		CHECK(upd(in, loc, 3, out, sizeof out));
		CHECK(sv(out) == sv("# my weather setup\nlatitude = 59.9100  # home\n\nunits = metric\nlog = info\n"
		    "longitude = 10.7500\nname = Oslo\n"));
	}
	{ // no trailing newline in the original
		CHECK(upd("units = metric", loc, 3, out, sizeof out));
		CHECK(sv(out) == sv("units = metric\nlatitude = 59.9100\nlongitude = 10.7500\nname = Oslo\n"));
	}
	{ // every duplicate is updated (the parser lets the last one win)
		CHECK(upd("name = A\nname = B\n", loc + 2, 1, out, sizeof out));
		CHECK(sv(out) == sv("name = Oslo\nname = Oslo\n"));
	}
	{ // CRLF line endings are kept
		CHECK(upd("name = A\r\nunits = metric\r\n", loc + 2, 1, out, sizeof out));
		CHECK(sv(out) == sv("name = Oslo\r\nunits = metric\r\n"));
	}
	{ // CR-only (classic Mac) files: lines are found, edited in place, and new keys use the file's line ending
		CHECK(upd("# my weather\rlatitude = 1.0\rname = X # home\runits = metric\r", loc, 3, out, sizeof out));
		CHECK(sv(out) == sv("# my weather\rlatitude = 59.9100\rname = Oslo  # home\runits = metric\rlongitude = 10.7500\r"));
		Config c;
		CHECK(config_parse(out, &c).ok() && c.lat_e4 == 599100 && c.lon_e4 == 107500 && !strcmp(c.name, "Oslo") && c.metric);
	}
	{ // a commented-out key is not the key
		CHECK(upd("# latitude = 5\n", loc, 1, out, sizeof out));
		CHECK(sv(out) == sv("# latitude = 5\nlatitude = 59.9100\n"));
	}
	{ // empty file, and a key with spaces around it
		CHECK(upd("", loc, 3, out, sizeof out));
		CHECK(sv(out) == sv("latitude = 59.9100\nlongitude = 10.7500\nname = Oslo\n"));
		CHECK(upd("   name   =   A  \n", loc + 2, 1, out, sizeof out));
		CHECK(sv(out) == sv("name = Oslo\n"));
	}
	{ // result still parses and says what we wrote
		CHECK(upd("# c\nunits=metric\n", loc, 3, out, sizeof out));
		Config c;
		CHECK(config_parse(out, &c).ok() && c.lat_e4 == 599100 && c.lon_e4 == 107500 && c.metric && !strcmp(c.name, "Oslo"));
	}
	{ // refusals
		Status st;
		const KeyVal hash[] = { {"name", "Oslo # home"} };
		CHECK(!upd("", hash, 1, out, sizeof out, &st) && st.err == Err::Config);
		const KeyVal nl[] = { {"name", "a\nb"} };
		CHECK(!upd("", nl, 1, out, sizeof out, &st) && st.err == Err::Config);
		const KeyVal sp[] = { {"name", " lead"} };
		CHECK(!upd("", sp, 1, out, sizeof out, &st) && st.err == Err::Config);
		const KeyVal badkey[] = { {"a=b", "x"} };
		CHECK(!upd("", badkey, 1, out, sizeof out, &st) && st.err == Err::Config);
		const KeyVal badlat[] = { {"latitude", "999"} };
		CHECK(!upd("", badlat, 1, out, sizeof out, &st) && st.err == Err::Config);   // would not read back
		const KeyVal longname[] = { {"name", "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"} };
		CHECK(!upd("", longname, 1, out, sizeof out, &st) && st.err == Err::Config);
		char tiny[16];
		CHECK(!upd("", loc, 3, tiny, sizeof tiny, &st) && st.err == Err::Config);   // output too small
	}
}

// ---- settings: the dotfile store (temp directory)
static int count_entries(const char *dir)
{
	DIR *d = opendir(dir);
	int n = 0;
	while (d) { struct dirent *e = readdir(d); if (!e) break; if (e->d_name[0] != '.') n++; }
	if (d) closedir(d);
	return n;
}
static bool slurp(const char *path, char *buf, size_t cap)
{
	FILE *f = fopen(path, "rb");
	if (!f) return false;
	size_t n = fread(buf, 1, cap - 1, f);
	buf[n] = 0; fclose(f);
	return true;
}

static void test_settings_store()
{
	char dir[] = "/tmp/weather-test-XXXXXX";
	CHECK(mkdtemp(dir) != nullptr);
	char path[300], text[1024];
	snprintf(path, sizeof path, "%s/rc", dir);
	Config c;
	c.lat_e4 = 599100; c.lon_e4 = 107500; c.metric = true;
	snprintf(c.name, sizeof c.name, "Oslo");

	PosixStore st;
	CHECK(posix_settings_init(&st, path, false).ok());
	size_t n = 99;
	CHECK(st.base.load(st.base, text, sizeof text, &n).ok() && n == 0);          // missing = empty
	PosixStore strict;
	CHECK(posix_settings_init(&strict, path, true).ok());
	CHECK(strict.base.load(strict.base, text, sizeof text, &n).err == Err::Config);   // explicit file must exist

	CHECK(settings_save(st.base, c).ok());                                      // creates it
	CHECK(slurp(path, text, sizeof text) && sv(text) == sv("latitude = 59.9100\nlongitude = 10.7500\nname = Oslo\nunits = metric\n"));
	CHECK(count_entries(dir) == 1);                                             // no temp files left behind
	CHECK(settings_save(st.base, c).ok());                                      // idempotent
	char again[1024];
	CHECK(slurp(path, again, sizeof again) && !strcmp(again, text));

	// user edits survive; mode is kept
	FILE *f = fopen(path, "wb"); fputs("# mine\nlog = info\nname = Old\n", f); fclose(f);
	chmod(path, 0600);
	CHECK(settings_save(st.base, c).ok());
	CHECK(slurp(path, text, sizeof text) && strstr(text, "# mine\nlog = info\nname = Oslo\n") == text);
	struct stat sb;
	CHECK(stat(path, &sb) == 0 && (sb.st_mode & 0777) == 0600);

	// a symlinked dotfile stays a symlink and the real file is updated
	char real[300], link[300];
	snprintf(real, sizeof real, "%s/real", dir);
	snprintf(link, sizeof link, "%s/link", dir);
	f = fopen(real, "wb"); fputs("name = Old\n", f); fclose(f);
	CHECK(symlink("real", link) == 0);
	PosixStore ls;
	CHECK(posix_settings_init(&ls, link, true).ok());
	CHECK(settings_save(ls.base, c).ok());
	struct stat lsb;
	CHECK(lstat(link, &lsb) == 0 && S_ISLNK(lsb.st_mode));
	CHECK(slurp(real, text, sizeof text) && strstr(text, "name = Oslo") != nullptr);

	// a directory we cannot write to: a clean Io error, original untouched
	if (geteuid() != 0) {
		char sub[300], subrc[320];
		snprintf(sub, sizeof sub, "%s/ro", dir);
		snprintf(subrc, sizeof subrc, "%s/rc", sub);
		CHECK(mkdir(sub, 0700) == 0);
		PosixStore ro;
		CHECK(posix_settings_init(&ro, subrc, false).ok());
		chmod(sub, 0500);
		Status s = settings_save(ro.base, c);
		CHECK(s.err == Err::Io);
		chmod(sub, 0700);
		unlink(subrc); rmdir(sub);
	}
	// cleanup (files only: we made them)
	unlink(path); unlink(real); unlink(link);
	CHECK(rmdir(dir) == 0);
}

// ---- app model
static Status fail_dns() { return make(Err::Dns, "cannot resolve host name"); }
static Forecast some_forecast(long t10)
{
	Forecast f; f.source = "open-meteo"; f.temp_t10 = t10; f.code = 2; f.humidity = 50;
	strcpy(f.time, "2026-10-24T05:00");
	return f;
}

static void test_model()
{
	ModelConfig mc;                    // 15 min interval, 60 s backoff base
	{ // first tick fetches; no second fetch while one is running; refresh during a fetch is ignored
		Model m(mc, 0);
		CHECK(m.phase() == Phase::Idle && !m.has_forecast());
		CHECK(m.tick(0) && m.phase() == Phase::Fetching);
		CHECK(!m.tick(1));
		m.request_refresh();
		CHECK(!m.tick(2));
		Forecast f = some_forecast(548);
		m.finish(5, okay(), &f);
		CHECK(m.phase() == Phase::Ok && m.has_forecast() && m.forecast().temp_t10 == 548);
		CHECK(!m.tick(6));             // the ignored refresh did not queue another fetch
		CHECK(m.age(65) == 60 && !m.stale(65));
		CHECK(!m.tick(904) && m.tick(905));            // 5 + 900
	}
	{ // failures back off 60, 120, 240 ... capped at the interval; success resets
		Model m(mc, 0);
		long long t = 0;
		long expect[] = {60, 120, 240, 480, 900, 900};
		for (long e : expect) {
			CHECK(m.tick(t));
			m.finish(t, fail_dns(), nullptr);
			CHECK(m.phase() == Phase::Failed && m.next_refresh_at() == t + e);
			CHECK(!m.tick(t + e - 1));
			t += e;
		}
		CHECK(m.failures() == 6);
		CHECK(m.tick(t));
		Forecast f = some_forecast(500);
		m.finish(t, okay(), &f);
		CHECK(m.failures() == 0 && m.next_refresh_at() == t + 900);
	}
	{ // a failure keeps the last good data, marked stale
		Model m(mc, 0);
		m.tick(0);
		Forecast f = some_forecast(548);
		m.finish(0, okay(), &f);
		CHECK(m.tick(900));
		m.finish(900, fail_dns(), nullptr);
		CHECK(m.phase() == Phase::Failed && m.has_forecast() && m.forecast().temp_t10 == 548 && m.stale(900));
		Buf<200> b; m.status_line(b, 930);     // data is from t=0, retry due at 960
		CHECK(b.view() == sv("Update failed: cannot resolve host name. Showing data from 16 min ago. Next update in 30 s"));
	}
	{ // a permanent error (bad certificate) is not retried faster than the normal interval
		Model m(mc, 0);
		m.tick(0);
		m.finish(0, make(Err::CertUntrusted, "certificate expired or not yet valid"), nullptr);
		CHECK(m.next_refresh_at() == 900);
	}
	{ // cancel is not a failure and does not back off
		Model m(mc, 0);
		m.tick(0);
		Forecast f = some_forecast(548);
		m.finish(0, okay(), &f);
		m.request_refresh();
		CHECK(m.tick(10));
		m.finish(12, make(Err::Cancelled, "cancelled"), nullptr);
		CHECK(m.phase() == Phase::Ok && m.failures() == 0 && m.has_forecast());
		Model first(mc, 0);
		first.tick(0);
		first.finish(1, make(Err::Cancelled, "cancelled"), nullptr);
		CHECK(first.phase() == Phase::Idle && first.failures() == 0);
	}
	{ // auto-refresh off: only manual refresh fetches; no countdown
		Model m(mc, 0);
		m.tick(0);
		Forecast f = some_forecast(548);
		m.finish(0, okay(), &f);
		m.set_auto_refresh(false, 1);
		CHECK(!m.tick(100000) && m.seconds_until_refresh(100) == -1);
		Buf<120> b; m.status_line(b, 300);
		CHECK(b.view() == sv("Updated 5 min ago"));
		m.request_refresh();
		CHECK(m.tick(100001));
		m.finish(100002, okay(), &f);
		m.set_auto_refresh(true, 100003);
		CHECK(m.seconds_until_refresh(100003) == 900);
	}
	{ // the clock jumped back by years: the next refresh is not postponed that long
		Model m(mc, 1000000);
		m.tick(1000000);
		Forecast f = some_forecast(548);
		m.finish(1000000, okay(), &f);
		CHECK(!m.tick(500));
		CHECK(m.next_refresh_at() <= 500 + 900);
		CHECK(m.tick(500 + 900));
		CHECK(m.age(100) == 0);          // never a negative age
	}
	{ // the clock stepped back after a fetch: the age of the data cannot be known. It is stale and must not read "just now"
		Model m(mc, 1000000);
		m.tick(1000000);
		Forecast f = some_forecast(548);
		m.finish(1000000, okay(), &f);
		CHECK(!m.stale(1000010));        // normal case unchanged
		Buf<200> ok; m.status_line(ok, 1000010);
		CHECK(strstr(ok.c_str(), "Updated just now") != nullptr);
		CHECK(m.stale(500));
		Buf<200> b; m.status_line(b, 500);
		CHECK(strstr(b.c_str(), "just now") == nullptr && strstr(b.c_str(), "Updated at an unknown time, clock changed") != nullptr);
		CHECK(m.stale(1000000 - 1) && !m.stale(1000000));   // the boundary: now == fetch time is a known age (0)
		CHECK(!m.tick(500));                                // the UI ticks every second; the refresh time is clamped to the new clock
		CHECK(m.tick(500 + 900));                           // fetching: the old data is still described honestly
		Buf<200> fb; m.status_line(fb, 500 + 900);
		CHECK(strstr(fb.c_str(), "showing data from an unknown time, clock changed") != nullptr);
		m.finish(500 + 900, make(Err::Timeout, "timed out"), nullptr);
		Buf<200> xb; m.status_line(xb, 500 + 901);
		CHECK(strstr(xb.c_str(), "Showing data from an unknown time, clock changed") != nullptr);
		CHECK(m.stale(500 + 901));
		m.tick(500 + 900 + 60);
		Forecast g = some_forecast(550);
		m.finish(500 + 900 + 61, okay(), &g);                // a fresh fetch under the new clock: known age again
		Buf<200> nb; m.status_line(nb, 500 + 900 + 62);
		CHECK(!m.stale(500 + 900 + 62) && strstr(nb.c_str(), "Updated just now") != nullptr);
	}
	{ // location changes while a fetch runs: its answer is dropped and a new fetch is queued
		Model m(mc, 0);
		m.tick(0);
		Forecast f = some_forecast(548);
		m.finish(0, okay(), &f);
		CHECK(m.tick(900));
		m.location_changed();
		Forecast old = some_forecast(999);
		m.finish(901, okay(), &old);
		CHECK(!m.has_forecast() && m.phase() == Phase::Idle);
		CHECK(m.tick(901));              // refetch for the new place right away
	}
	{ // location changes while idle
		Model m(mc, 0);
		m.tick(0);
		Forecast f = some_forecast(548);
		m.finish(0, okay(), &f);
		m.location_changed();
		CHECK(!m.has_forecast() && m.tick(1));
	}
	{ // status text
		Model m(mc, 0);
		Buf<160> b;
		m.status_line(b, 0);
		CHECK(b.view() == sv("Waiting to load"));
		m.tick(0);
		Buf<160> b2; m.status_line(b2, 1);
		CHECK(b2.view() == sv("Fetching..."));
		Forecast f = some_forecast(548);
		m.finish(2, okay(), &f);
		Buf<160> b3; m.status_line(b3, 2);
		CHECK(b3.view() == sv("Updated just now. Next update in 15 min"));
		Buf<160> b4; m.status_line(b4, 2 + 300);
		CHECK(b4.view() == sv("Updated 5 min ago. Next update in 10 min"));
		Buf<160> b5; m.status_line(b5, 2 + 3 * 3600 + 120);
		CHECK(b5.view() == sv("Updated 3 h 2 min ago. Next update in 0 s"));   // overdue until tick() fires
	}
}

static void test_forms()
{
	{ // text editing: insert at the cursor, delete both ways, cursor limits, capacity, non-printables refused
		TextEdit<8> t;
		CHECK(t.insert('a') && t.insert('c') && t.view() == sv("ac"));
		t.left();
		CHECK(t.insert('b') && t.view() == sv("abc") && t.cur == 2);
		CHECK(t.del() && t.view() == sv("ab"));
		CHECK(t.backspace() && t.view() == sv("a") && t.cur == 1);
		t.home(); CHECK(!t.backspace() && t.cur == 0);
		t.end(); CHECK(!t.del() && t.cur == 1);
		t.right(); CHECK(t.cur == 1);
		CHECK(!t.insert('\n') && !t.insert('\t') && !t.insert(static_cast<char>(200)));
		t.set("0123456789");                       // longer than capacity: cut, stays terminated
		CHECK(t.n == 7 && t.s[7] == 0 && !t.insert('x'));
		t.set("a\tb");
		CHECK(t.view() == sv("ab"));
		t.set_cursor(99); CHECK(t.cur == t.n);
	}
	{ // location form: load, edit, validate
		Config c;
		LocationForm f;
		f.load(c);
		CHECK(f.lat.view() == sv("43.0747") && f.lon.view() == sv("-89.3841") && f.name.view() == sv("Madison, WI") && !f.metric);
		f.lat.set("59.91"); f.lon.set("10.75"); f.name.set("  Oslo ");
		f.metric = true;
		CHECK(f.read(&c).ok() && c.lat_e4 == 599100 && c.lon_e4 == 107500 && !strcmp(c.name, "Oslo") && c.metric);
		Config before = c;
		f.lat.set("91");
		CHECK(f.read(&c).err == Err::Config && f.focus == LocationForm::Lat && f.error[0] != 0);
		CHECK(c.lat_e4 == before.lat_e4);                         // untouched on failure
		f.lat.set("12"); f.lon.set("abc");
		CHECK(!f.read(&c).ok() && f.focus == LocationForm::Lon);
		f.lon.set(""); CHECK(!f.read(&c).ok());
		f.lon.set("-0.1278"); f.name.set("a#b");
		CHECK(!f.read(&c).ok() && f.focus == LocationForm::Name);
		f.name.set("");                                            // an empty name is allowed (no label)
		CHECK(f.read(&c).ok() && c.name[0] == 0);
		f.focus = 0; f.next(); f.next(); f.next(); f.next();
		CHECK(f.focus == 0);
		f.prev(); CHECK(f.focus == LocationForm::Units);
	}
	{ // autorefresh key, and saving single keys without touching the rest
		Config c;
		CHECK(config_parse("autorefresh = off\n", &c).ok() && !c.auto_refresh);
		CHECK(config_parse("autorefresh = on\n", &c).ok() && c.auto_refresh);
		CHECK(config_parse("autorefresh = maybe\n", &c).err == Err::Config);
		char dir[] = "/tmp/weather-test-XXXXXX";
		CHECK(mkdtemp(dir) != nullptr);
		char path[300];
		snprintf(path, sizeof path, "%s/rc", dir);
		FILE *fp = fopen(path, "wb"); fputs("# hi\nname = X\n", fp); fclose(fp);
		PosixStore st;
		CHECK(posix_settings_init(&st, path, true).ok());
		KeyVal kv[] = { {"autorefresh", "off"} };
		CHECK(settings_save_kv(st.base, kv, 1).ok());
		char text[256];
		CHECK(slurp(path, text, sizeof text) && sv(text) == sv("# hi\nname = X\nautorefresh = off\n"));
		unlink(path);
		CHECK(rmdir(dir) == 0);
	}
	{ // provider lists
		Provider second = kOpenMeteo; second.name = "second";
		const Provider *all[] = { &kOpenMeteo, &second };
		const Provider *out[4];
		size_t n = 0;
		CHECK(resolve_providers("open-meteo", all, 2, out, 4, &n).ok() && n == 1 && out[0] == &kOpenMeteo);
		CHECK(resolve_providers("second, open-meteo", all, 2, out, 4, &n).ok() && n == 2 && out[0] == &second && out[1] == &kOpenMeteo);
		CHECK(resolve_providers("open-meteo,,", all, 2, out, 4, &n).ok() && n == 1);
		CHECK(resolve_providers("nope", all, 2, out, 4, &n).err == Err::Usage);
		CHECK(resolve_providers("", all, 2, out, 4, &n).err == Err::Usage);
		CHECK(resolve_providers("open-meteo,second", all, 2, out, 1, &n).err == Err::Usage);   // too many
	}
}

static void test_metno()
{
	Query q;
	Forecast f;
	Status s = metno_parse(fixture(0, "metno-ok.json"), q, &f);
	CHECK(s.ok() && f.temp_t10 == 545 && f.wind_t10 == 78 && f.humidity == 77 && f.wind_dir == 161 && f.code == 2 && f.is_day == 0);
	CHECK(!strcmp(f.time, "2026-10-24T06:00") && !strcmp(f.source, "met-no") && !f.metric);
	Buf<200> one;
	format_oneline(one, f);
	CHECK(one.view() == sv("54.5F Partly cloudy, wind 7.8mph S, humidity 77% (met-no 2026-10-24T06:00)"));
	q.metric = true;
	Forecast m;
	CHECK(metno_parse(fixture(0, "metno-ok.json"), q, &m).ok() && m.temp_t10 == 125 && m.wind_t10 == 126 && m.metric);

	Forecast g;
	CHECK(metno_parse(R"({"properties":{"timeseries":[{"time":"t","data":{"instant":{"details":{"air_temperature":"warm"}}}}]}})", Query{}, &g).err == Err::BadData);
	CHECK(metno_parse(R"({"properties":{"timeseries":[{"time":"t","data":{"instant":{"details":{"wind_speed":3.5}}}}]}})", Query{}, &g).err == Err::BadData);
	CHECK(metno_parse(R"({"properties":{"timeseries":[{"data":{"instant":{"details":{"air_temperature":5.0}}}}]}})", Query{}, &g).err == Err::BadData);   // no symbol, no cloud cover
	CHECK(metno_parse(R"({"properties":{"timeseries":[{"data":{"instant":{"details":{"air_temperature":5.0,"cloud_area_fraction":80.0}}}}]}})", Query{}, &g).ok() && g.code == 3);   // cloud fallback
	CHECK(metno_parse(R"({"properties":{"timeseries":[{"data":{"instant":{"details":{"air_temperature":999}}}}]}})", Query{}, &g).err == Err::BadData);   // implausible
	CHECK(metno_parse("<html>", Query{}, &g).err == Err::Parse);

	char path[200];
	Query lq; lq.lat_e4 = 430747; lq.lon_e4 = -893841;
	CHECK(kMetNo.path(lq, path, sizeof path) && !strcmp(path, "/weatherapi/locationforecast/2.0/compact?lat=43.0747&lon=-89.3841"));
	CHECK(kMetNo.needs_contact && !kOpenMeteo.needs_contact);

	// contact: refused before touching the network; with one it goes into the User-Agent
	char buf[4096];
	FetchOpts o; o.attempts = 1; o.force_host = "fake"; o.force_port = "80";
	const Provider *only[] = { &kMetNo };
	Forecast out;
	{
		Fake fk; fk.steps[0] = {okay(), kGood, Err::Ok}; fk.nsteps = 1;
		Net n = make_net(&fk);
		Status r = fetch_forecast(n, only, 1, lq, o, buf, sizeof buf, &out);
		CHECK(r.err == Err::Config && fk.connects == 0);
	}
	static char metresp[4096];
	snprintf(metresp, sizeof metresp, "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n\r\n%s", fixture(1, "metno-ok.json"));
	o.contact = "tester@example.org";
	{
		Fake fk; fk.steps[0] = {okay(), metresp, Err::Ok}; fk.nsteps = 1;
		Net n = make_net(&fk);
		Status r = fetch_forecast(n, only, 1, lq, o, buf, sizeof buf, &out);
		CHECK(r.ok() && out.temp_t10 == 545);
		CHECK(strstr(fk.request, "User-Agent: weather/0.1.0 (contact: tester@example.org)\r\n") != nullptr);
		CHECK(strstr(fk.request, "Host: fake\r\n") != nullptr);
	}
	{ // Open-Meteo fails, MET Norway answers
		const Provider *two[] = { &kOpenMeteo, &kMetNo };
		Fake fk; fk.steps[0] = {okay(), k500, Err::Ok}; fk.steps[1] = {okay(), metresp, Err::Ok}; fk.nsteps = 2;
		Net n = make_net(&fk);
		Status r = fetch_forecast(n, two, 2, lq, o, buf, sizeof buf, &out);
		CHECK(r.ok() && !strcmp(out.source, "met-no") && fk.connects == 2);
	}
	{ // MET Norway is skipped without a contact and the next source is used
		const Provider *two[] = { &kMetNo, &kOpenMeteo };
		FetchOpts o2; o2.attempts = 1; o2.force_host = "fake"; o2.force_port = "80";
		static char omresp[4096];
		snprintf(omresp, sizeof omresp, "HTTP/1.0 200 OK\r\n\r\n%s", fixture(0, "openmeteo-ok.json"));
		Fake fk; fk.steps[0] = {okay(), omresp, Err::Ok}; fk.nsteps = 1;
		Net n = make_net(&fk);
		Status r = fetch_forecast(n, two, 2, lq, o2, buf, sizeof buf, &out);
		CHECK(r.ok() && !strcmp(out.source, "open-meteo") && fk.connects == 1);
	}
	// config key
	Config c;
	CHECK(config_parse("contact = jack@example.org\n", &c).ok() && !strcmp(c.contact, "jack@example.org"));
	CHECK(config_set(&c, "contact", "a#b", 1).err == Err::Config);        // (in a file, # starts a comment)
	CHECK(config_parse("contact = a(b)\n", &c).err == Err::Config);
	CHECK(config_parse("contact = abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n", &c).err == Err::Config);
	// the registry
	const Provider *res[4];
	size_t nres = 0;
	CHECK(resolve_providers("met-no,open-meteo", kProviders, kNumProviders, res, 4, &nres).ok() && nres == 2 && res[0] == &kMetNo);
}

// OS-independent lifecycle tests for the actual OS 9 resolver controller.
struct DnsFake {
	uint32_t clock = 0, open_at = 0, query_at = 0;
	uint32_t open_delay = 0, query_delay = 0;
	int opens = 0, queries = 0, closes = 0, yields = 0, cancel_at = 0;
	int open_error = 0, open_result_code = 0, query_error = 0, query_result_code = 0, close_error = 0;
	bool address = true;
	uint32_t now() { return clock; }
	bool yield() { return ++yields != cancel_at; }
	void sleep() { clock += 3; }
	int open(const char *) { ++opens; open_at = clock; return open_error; }
	bool opened() { return static_cast<uint32_t>(clock - open_at) >= open_delay; }
	int open_result() { return open_result_code; }
	int query() { ++queries; query_at = clock; return query_error; }
	bool resolved() { return static_cast<uint32_t>(clock - query_at) >= query_delay; }
	int query_result() { return query_result_code; }
	bool has_address() { return address; }
	int close() { ++closes; return close_error; }
};

static void test_os9_dns()
{
	{ DnsFake f; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 60).ok()); CHECK(f.opens == 1 && f.queries == 1 && f.closes == 1); }
	{ DnsFake f; f.open_delay = 6; f.query_delay = 9; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 60).ok()); CHECK(f.clock == 15 && f.yields > 2); }
	{ DnsFake f; f.open_error = -1; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 60).err == Err::Dns); CHECK(f.queries == 0 && f.closes == 0); }
	{ DnsFake f; f.open_result_code = -1; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 60).err == Err::Dns); CHECK(f.queries == 0 && f.closes == 1); }
	{ DnsFake f; f.query_error = -1; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 60).err == Err::Dns); CHECK(f.closes == 1); }
	{ DnsFake f; f.query_result_code = -1; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 60).err == Err::Dns); CHECK(f.closes == 1); }
	{ DnsFake f; f.address = false; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 60).err == Err::Dns); CHECK(f.closes == 1); }
	{ DnsFake f; f.query_delay = 100; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 6).err == Err::Timeout); CHECK(f.clock == 6 && f.closes == 1);
	  f.query_delay = 0; CHECK(r.resolve("h", 60).ok()); CHECK(f.closes == 2); }
	{ DnsFake f; f.cancel_at = 1; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 60).err == Err::Cancelled); CHECK(f.opens == 0); }
	{ DnsFake f; f.query_delay = 100; f.cancel_at = 4; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 60).err == Err::Cancelled); CHECK(f.closes == 1 && f.queries == 1); }
	{ // Pending open is retained; a retry drains it before starting another open.
	  DnsFake f; f.open_delay = 12; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("first", 6).err == Err::Timeout); CHECK(f.closes == 0 && f.queries == 0);
	  CHECK(r.resolve("second", 60).ok()); CHECK(f.opens == 2 && f.closes == 2 && f.queries == 1); }
	{ // Repeated cancellation while the same open is still pending never reuses its context.
	  DnsFake f; f.open_delay = 100; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("first", 3).err == Err::Timeout);
	  f.cancel_at = f.yields + 1; CHECK(r.resolve("second", 60).err == Err::Cancelled);
	  CHECK(f.opens == 1 && f.closes == 0); }
	{ // A failed close quarantines the backend rather than reusing live OT storage.
	  DnsFake f; f.close_error = -1; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 60).err == Err::Dns);
	  CHECK(r.resolve("h", 60).err == Err::Dns); CHECK(f.opens == 1 && f.closes == 1); }
	{ // Expiry wins over simultaneous completion.
	  DnsFake f; f.query_delay = 6; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 6).err == Err::Timeout); }
	{ // Cancellation wins over simultaneous expiry and completion.
	  DnsFake f; f.query_delay = 6; f.cancel_at = 5; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 6).err == Err::Cancelled); }
	{ DnsFake f; f.clock = UINT32_MAX - 2; f.query_delay = 6; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 12).ok()); CHECK(f.clock == 3); }
	{ DnsFake f; DnsResolver<DnsFake> r(f);
	  CHECK(r.resolve("h", 0).err == Err::Timeout); CHECK(f.opens == 0); }
}

int main()
{
	log_init(LogError, false, "unittest");   // expected failures are noisy; CHECK reports real ones
	test_openmeteo();
	test_config();
	test_fetch();
	test_fallback();
	test_cancel();
	test_posix_net();
	test_settings_text();
	test_settings_store();
	test_model();
	test_forms();
	test_metno();
	test_os9_dns();
	test_seed_pool();
	test_os9_helpers();
	printf("%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}
