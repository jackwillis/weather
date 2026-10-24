// Unit tests: plain C++, no framework. Build-time checks (static_assert) come first:
// if the core is wrong, this file does not compile.
#include "core/config.hpp"
#include "core/fetch.hpp"
#include "core/http.hpp"
#include "core/json.hpp"
#include "core/log.hpp"
#include "core/openmeteo.hpp"
#include "core/version.hpp"
#include <cstdio>
#include <cstring>

using namespace wx;

// ---- compile-time tests (run by the compiler, so also valid for big-endian targets)
static_assert(parse_fixed("54.8", 1).v == 548, "fixed");
static_assert(parse_fixed("-3", 1).v == -30, "neg int");
static_assert(parse_fixed("41.87949", 4).v == 418795, "round up");
static_assert(parse_fixed("41.87944", 4).v == 418794, "round down");
static_assert(!parse_fixed("1e5", 0).ok, "no exponent");
static_assert(!parse_fixed("", 0).ok && !parse_fixed("-", 0).ok && !parse_fixed(".5", 0).ok, "empty");
static_assert(!parse_fixed("1.", 1).ok, "trailing dot");
static_assert(!parse_fixed("1234567890", 0).ok, "too many digits");

constexpr bool buf_ok()
{
	Buf<16> b;
	b.add_fixed(-5, 1).add(' ').add_fixed(548, 1).add(' ').add_int(0);
	return b.view() == sv("-0.5 54.8 0") && !b.truncated;
}
static_assert(buf_ok(), "buf");
constexpr bool buf_trunc()
{
	Buf<4> b;
	b.add("abcdef");
	return b.view() == sv("abc") && b.truncated;
}
static_assert(buf_trunc(), "buf truncation");

// The Open-Meteo gotcha: the units object repeats every key with a string.
constexpr sv kTrap =
    R"({"current_units":{"temperature_2m":"F"},"current":{"time":"t","temperature_2m":54.8}})";
static_assert(json_num(json_get(json_get(kTrap, "current").value, "temperature_2m").value, 1).v == 548,
    "top-level key lookup ignores nested duplicates");
static_assert(json_get(kTrap, "temperature_2m").r == JsonFind::Absent, "no deep search");
static_assert(!json_num(R"("54.8")", 1).ok, "string is not a number");
static_assert(json_get("{\"a\":1", "a").r == JsonFind::Malformed, "truncated object");
static_assert(json_get("[1]", "a").r == JsonFind::Malformed, "not an object");

constexpr sv kHttp = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}";
static_assert(http_parse(kHttp).st.ok() && http_parse(kHttp).code == 200 && http_parse(kHttp).body == sv("{}"),
    "http ok");
static_assert(http_parse("HTTP/1.1 200 OK\r\nContent-Length: 9\r\n\r\n{}").st.err == Err::Truncated, "short body");
static_assert(http_parse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\n{}\r\n0\r\n\r\n").st.err == Err::BadData,
    "chunked refused");
static_assert(http_parse("garbage garbage").st.err == Err::Parse, "not http");
static_assert(http_parse("HTTP/1.1 200 OK\r\nContent-Le").st.err == Err::Truncated, "cut headers");
static_assert(http_parse("HTTP/1.1 503 x\r\n\r\n").code == 503, "status code");

static_assert(compass(0) == sv("N") && compass(90) == sv("E") && compass(138) == sv("SE") &&
    compass(350) == sv("N") && compass(-45) == sv("NW"), "compass");

// ---- runtime tests
static int g_fail = 0, g_run = 0;
#define CHECK(c) do { g_run++; if (!(c)) { g_fail++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

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
	s = config_parse("name = abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n", &c);
	CHECK(s.err == Err::Config);                            // too long
	CHECK(config_parse("", &c).ok());
	{ Config d; CHECK(d.lat_e4 == 430747 && d.lon_e4 == -893841 && !strcmp(d.name, "Madison, WI")); }
	CHECK(config_parse("units = metric", &c).ok());         // no trailing newline
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
	long sleeps[8] = {};
	long clock = 1790000000;
	const char *cur = ""; size_t pos = 0; Err cur_err = Err::Ok;
	char request[512] = {};
};
static Fake *F(Net &n) { return static_cast<Fake *>(n.ctx); }

static long fk_read(Io &io, void *buf, size_t len)
{
	Fake *f = static_cast<Fake *>(io.ctx);
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
static void fk_sleep(Net &n, long ms) { Fake *f = F(n); if (f->nsleeps < 8) f->sleeps[f->nsleeps++] = ms; }
static long fk_now(Net &n) { return F(n)->clock; }

static Net make_net(Fake *f)
{
	Net n;
	n.ctx = f; n.connect = fk_connect; n.close = fk_close; n.sleep_ms = fk_sleep; n.now = fk_now;
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

int main()
{
	log_init(LogError, false, "unittest");   // expected failures are noisy; CHECK reports real ones
	test_openmeteo();
	test_config();
	test_fetch();
	test_fallback();
	printf("%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}
