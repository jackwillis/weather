// Parser robustness: deterministic mutation testing. Every parser sees truncated, byte-flipped,
// byte-deleted and randomly edited versions of real inputs, plus hostile shapes (deep nesting,
// huge numbers, embedded NULs). The point is "never crashes, never reads out of range, and if it
// says OK the answer is sane"; build it with -fsanitize=address,undefined (make check-san) and the
// sanitizers do the watching. usage: robust [random-iterations] (default 20000)
#include "app/textedit.hpp"
#include "core/config.hpp"
#include "core/http.hpp"
#include "core/json.hpp"
#include "core/metno.hpp"
#include "core/openmeteo.hpp"
#include "core/settings.hpp"
#include "core/version.hpp"
#include "tls/seed_pool.hpp"
#include "platform/os9/portable.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace wx;

static int g_fail = 0;
static unsigned long g_runs = 0;
#define EXPECT(c) do { if (!(c)) { if (g_fail++ < 20) printf("FAIL %s:%d: %s (input %zu bytes)\n", __FILE__, __LINE__, #c, g_cur_len); } } while (0)
static size_t g_cur_len;

struct Rng {
	unsigned long long s = 0x9e3779b97f4a7c15ULL;
	unsigned next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return static_cast<unsigned>(s >> 16); }
	unsigned below(unsigned n) { return n ? next() % n : 0; }
};

static bool inside(sv outer, sv inner)
{
	return inner.data() >= outer.data() && inner.data() + inner.size() <= outer.data() + outer.size();
}

// Independent statement of what an accepted forecast may contain (literal bounds on purpose, not the parser's own predicates).
static bool sane(const Forecast &f, bool metric)
{
	bool t = metric ? (f.temp_t10 >= -900 && f.temp_t10 <= 600) : (f.temp_t10 >= -1300 && f.temp_t10 <= 1400);
	bool h = f.humidity == kUnset || (f.humidity >= 0 && f.humidity <= 100);
	bool w = f.wind_t10 == kUnset || (f.wind_t10 >= 0 && f.wind_t10 <= (metric ? 5000 : 3000));
	bool d = f.wind_dir == kUnset || (f.wind_dir >= 0 && f.wind_dir <= 360);
	bool day = f.is_day == kUnset || f.is_day == 0 || f.is_day == 1;
	return t && h && w && d && day && f.code >= 0 && f.code <= 99 && f.metric == metric;
}

static void exercise(const char *p, size_t n)
{
	g_runs++;
	g_cur_len = n;
	sv in(p, n);
	// Format adapter: bounded output, failure empties it, deterministic prefix.
	char full[257], small[18];
	memset(small, 0x5a, sizeof small);
	bool adapted = os9::rewrite_log_format(in, full, sizeof full);
	bool bounded = os9::rewrite_log_format(in, small, sizeof small - 1);
	EXPECT(small[17] == 0x5a);
	if (bounded) { EXPECT(adapted); EXPECT(strlen(small) < 17); EXPECT(!strcmp(full, small)); }
	else EXPECT(small[0] == 0);
	if (adapted) { EXPECT(strlen(full) == n); EXPECT(n < sizeof full); }
	// Compare full-width integer formatting with the host's independent formatter.
	unsigned long long number = 0;
	for (size_t i = 0; i < n && i < 8; ++i)
		number = (number << 8) | static_cast<unsigned char>(p[i]);
	char decimal[22], expected[22];
	EXPECT(os9::decimal_unsigned(number, decimal, sizeof decimal));
	snprintf(expected, sizeof expected, "%llu", number);
	EXPECT(!strcmp(decimal, expected));
	long long signed_number = static_cast<long long>(number >> 1);
	if (number & 1) signed_number = -signed_number;
	EXPECT(os9::decimal_signed(signed_number, decimal, sizeof decimal));
	snprintf(expected, sizeof expected, "%lld", signed_number);
	EXPECT(!strcmp(decimal, expected));
	unsigned char seed_out[32], encoded[kSeedRecordSize];
	if (seed_decode(reinterpret_cast<const unsigned char *>(p), n, seed_out)) {
		unsigned nonzero = 0;
		for (unsigned char c : seed_out) nonzero |= c;
		EXPECT(nonzero != 0);
		seed_encode(seed_out, encoded);
		EXPECT(n == kSeedRecordSize && std::memcmp(p, encoded, n) == 0);
	}

	// HTTP
	HttpResponse h = http_parse(in);
	if (h.st.ok()) { EXPECT(inside(in, h.body)); EXPECT(h.code >= 0 && h.code <= 999); }

	// JSON navigation
	static const char *keys[] = {"current", "temperature_2m", "error", "a", ""};
	for (const char *k : keys) {
		JsonLookup l = json_get(in, k);
		if (l.r == JsonFind::Found) {
			EXPECT(inside(in, l.value) && !l.value.empty());
			EXPECT(json_skip(l.value, 0) == l.value.size());          // exactly one JSON value
		}
	}
	size_t e = json_skip(in, 0);
	if (e != sv::npos) EXPECT(e <= n);

	// Open-Meteo
	Query q;
	Forecast f;
	Status s = openmeteo_parse(in, q, &f);
	if (s.ok()) {
		EXPECT(plausible_temp_t10(f.temp_t10, false));
		EXPECT(sane(f, false));
		EXPECT(f.code != kUnset && std::strlen(f.time) < sizeof f.time);
		Buf<300> one; format_oneline(one, f);                         // formatting any accepted forecast must be safe
		Buf<400> full; format_full(full, f, "Madison, WI");
	}
	Forecast mq;                                                  // the same bytes asked for metric: accepted only if sane as metric
	if (openmeteo_parse(in, Query{0, 0, true}, &mq).ok()) EXPECT(sane(mq, true));
	Forecast mf;                                                  // MET Norway: same rules
	Status ms = metno_parse(in, q, &mf);
	if (ms.ok()) {
		EXPECT(plausible_temp_t10(mf.temp_t10, false) && mf.code != kUnset && std::strlen(mf.time) < sizeof mf.time);
		EXPECT(sane(mf, false));
		Buf<300> one; format_oneline(one, mf);
	}
	Forecast mm;
	if (metno_parse(in, Query{0, 0, true}, &mm).ok()) EXPECT(sane(mm, true));

	// numbers
	for (int d = 0; d <= 4; d++) { Num x = parse_fixed(in.substr(0, 40), d); if (x.ok) EXPECT(x.v > -2000000000L && x.v < 2000000000L); }
	for (int d = 5; d <= 9; d++) (void)parse_fixed(in.substr(0, 40), d);
	(void)json_num(in.substr(0, 40), 1);

	// config file and settings editor
	Config c;
	Status cs = config_parse(in, &c);
	if (cs.ok()) {
		EXPECT(c.lat_e4 >= -900000 && c.lat_e4 <= 900000 && c.lon_e4 >= -1800000 && c.lon_e4 <= 1800000);
		EXPECT(c.timeout_s >= 1 && c.timeout_s <= 300 && std::strlen(c.name) < sizeof c.name && std::strlen(c.providers) < sizeof c.providers);
	}
	static char out[2048];
	size_t on = 0;
	const KeyVal kv[] = { {"latitude", "43.0747"}, {"longitude", "-89.3841"}, {"name", "Madison, WI"}, {"units", "metric"} };
	Status us = settings_update(in, kv, 4, out, sizeof out, &on);
	if (us.ok()) {
		EXPECT(on < sizeof out && out[on] == 0);
		Config back;
		EXPECT(config_parse(sv(out, on), &back).ok());               // never write what we cannot read
		EXPECT(back.lat_e4 == 430747 && back.lon_e4 == -893841 && back.metric && !std::strcmp(back.name, "Madison, WI"));
		size_t on2 = 0;
		static char out2[2048];
		EXPECT(settings_update(sv(out, on), kv, 4, out2, sizeof out2, &on2).ok() && sv(out2, on2) == sv(out, on));   // idempotent
	}

	// text field fed as a stream of keystrokes
	TextEdit<12> t;
	for (size_t i = 0; i < n && i < 400; i++) {
		unsigned char ch = static_cast<unsigned char>(p[i]);
		switch (ch % 9) {
		case 0: t.backspace(); break;
		case 1: t.del(); break;
		case 2: t.left(); break;
		case 3: t.right(); break;
		case 4: t.home(); break;
		case 5: t.end(); break;
		default: t.insert(static_cast<char>(ch));
		}
		if (!(t.n < 12 && t.cur <= t.n && t.s[t.n] == 0)) { EXPECT(false); break; }
	}
	t.set(in);
	EXPECT(t.n < 12 && t.cur == t.n && t.s[t.n] == 0);
}

static char g_buf[16384];

static void mutate_all(const char *seed, size_t n, unsigned iterations, Rng &rng)
{
	exercise(seed, n);
	for (size_t k = 0; k <= n; k++) exercise(seed, k);                         // every truncation
	static const unsigned char repl[] = {0, '"', '{', '}', '[', ']', ',', ':', '\\', '\n', '\r', 0xff, '9', '-', '.', ' ', '#', '='};
	for (size_t i = 0; i < n; i++) {                                           // every single-byte replacement
		memcpy(g_buf, seed, n);
		for (unsigned char r : repl) { g_buf[i] = static_cast<char>(r); exercise(g_buf, n); }
	}
	for (size_t i = 0; i < n; i++) {                                           // every single-byte deletion
		memcpy(g_buf, seed, i);
		memcpy(g_buf + i, seed + i + 1, n - i - 1);
		exercise(g_buf, n - 1);
	}
	for (unsigned it = 0; it < iterations; it++) {                             // random edits
		size_t m = n;
		memcpy(g_buf, seed, n);
		unsigned edits = 1 + rng.below(4);
		for (unsigned e = 0; e < edits && m > 0; e++) {
			size_t pos = rng.below(static_cast<unsigned>(m));
			switch (rng.below(5)) {
			case 0: g_buf[pos] = static_cast<char>(rng.next()); break;
			case 1: if (m + 1 < sizeof g_buf) { memmove(g_buf + pos + 1, g_buf + pos, m - pos); g_buf[pos] = static_cast<char>(rng.next()); m++; } break;
			case 2: memmove(g_buf + pos, g_buf + pos + 1, m - pos - 1); m--; break;
			case 3: { size_t len = 1 + rng.below(40); if (pos + len > m) len = m - pos;
			          if (m + len < sizeof g_buf) { memmove(g_buf + pos + len, g_buf + pos, m - pos); m += len; } break; }   // duplicate a chunk
			default: { size_t len = 1 + rng.below(20); if (pos + len > m) len = m - pos; memmove(g_buf + pos, g_buf + pos + len, m - pos - len); m -= len; break; }
			}
		}
		exercise(g_buf, m);
	}
}

static bool slurp(const char *path, char *buf, size_t cap, size_t *n)
{
	FILE *f = fopen(path, "rb");
	if (!f) return false;
	*n = fread(buf, 1, cap, f);
	fclose(f);
	return true;
}

int main(int argc, char **argv)
{
	unsigned iterations = argc > 1 ? static_cast<unsigned>(atoi(argv[1])) : 20000;
	Rng rng;
	static char seed[8192];
	size_t n = 0;
	char path[256];
	const char *fixtures[] = {"openmeteo-ok.json", "openmeteo-400.json", "metno-ok.json"};
	for (const char *fx : fixtures) {
		snprintf(path, sizeof path, "%s/%s", FIXTURE_DIR, fx);
		if (!slurp(path, seed, sizeof seed, &n)) { printf("cannot read %s\n", path); return 1; }
		mutate_all(seed, n, iterations, rng);
	}
	// an HTTP response wrapped around the real fixture, a config file, and a settings file
	static char http[9000];
	snprintf(path, sizeof path, "%s/openmeteo-ok.json", FIXTURE_DIR);
	slurp(path, seed, sizeof seed, &n);
	int hl = snprintf(http, sizeof http, "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n", n);
	memcpy(http + hl, seed, n);
	mutate_all(http, static_cast<size_t>(hl) + n, iterations, rng);
	static const char cfg[] = "# my weather\nlatitude = 43.0747  # home\nlongitude=-89.3841\nname = Madison, WI\nunits = metric\n"
	    "timeout = 15\nattempts = 3\nproviders = open-meteo\nlog = info\nautorefresh = off\n";
	mutate_all(cfg, sizeof cfg - 1, iterations, rng);
	static char crcfg[sizeof cfg];                                          // the same file with classic Mac line endings
	for (size_t i = 0; i < sizeof cfg - 1; i++) crcfg[i] = cfg[i] == '\n' ? '\r' : cfg[i];
	mutate_all(crcfg, sizeof cfg - 1, iterations, rng);
	static const char formats[] = "%zu %%zu %08zu %.*zu %lld %s %ld %.2f %%";
	mutate_all(formats, sizeof formats - 1, iterations, rng);
	unsigned char synthetic_seed[32], record[kSeedRecordSize];
	for (size_t i = 0; i < sizeof synthetic_seed; ++i) synthetic_seed[i] = static_cast<unsigned char>(i + 1);
	seed_encode(synthetic_seed, record);
	mutate_all(reinterpret_cast<const char *>(record), sizeof record, iterations, rng);
	memset(synthetic_seed, 0, sizeof synthetic_seed);
	seed_encode(synthetic_seed, record);
	mutate_all(reinterpret_cast<const char *>(record), sizeof record, iterations, rng);

	// hostile shapes
	static char big[20000];
	memset(big, '[', sizeof big);
	exercise(big, sizeof big);                                              // 20000 nested arrays: the depth limit, not the stack
	memset(big, '{', sizeof big);
	exercise(big, sizeof big);
	for (size_t i = 0; i + 6 < sizeof big; i += 6) memcpy(big + i, "{\"a\":[", 6);
	exercise(big, sizeof big);
	memset(big, '9', sizeof big);
	exercise(big, sizeof big);                                              // a number with 20000 digits
	const char nul[] = "{\"current\":{\"temperature_2m\":5\0" "4.8,\"weather_code\":0}}";
	exercise(nul, sizeof nul - 1);                                          // embedded NUL
	char all[256];
	for (int i = 0; i < 256; i++) all[i] = static_cast<char>(i);
	exercise(all, sizeof all);

	printf("robust: %lu inputs, %d failed\n", g_runs, g_fail);
	return g_fail ? 1 : 0;
}
