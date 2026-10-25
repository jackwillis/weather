// Compile-time tests of the portable core. If the core is wrong on a compiler, this does not compile there.
// Included by unittest.cpp (host and maul) and by the OS 9 probe (Retro68): a bad parser breaks the build, on every target.
#pragma once
#include "platform/os9/portable.hpp"
#include "core/config.hpp"
#include "core/forecast.hpp"
#include "core/http.hpp"
#include "core/json.hpp"
#include "core/metno.hpp"
#include "core/openmeteo.hpp"
#include "core/settings.hpp"
#include "app/locform.hpp"
#include "app/model.hpp"
#include "app/textedit.hpp"

namespace wx_static_tests {
using namespace wx;

static_assert(parse_fixed("54.8", 1).v == 548, "fixed");
static_assert(parse_fixed("-3", 1).v == -30, "neg int");
static_assert(parse_fixed("41.87949", 4).v == 418795, "round up");
static_assert(parse_fixed("41.87944", 4).v == 418794, "round down");
static_assert(!parse_fixed("1e5", 0).ok, "no exponent");
static_assert(!parse_fixed("", 0).ok && !parse_fixed("-", 0).ok && !parse_fixed(".5", 0).ok, "empty");
static_assert(!parse_fixed("1.", 1).ok, "trailing dot");
static_assert(!parse_fixed("1234567890", 0).ok, "too many digits");
static_assert(!parse_fixed("999999999.0000", 4).ok || sizeof(long) > 4, "reject scaled overflow on 32-bit");
static_assert(!parse_fixed("214748364.75", 1).ok || sizeof(long) > 4, "rounding overflow on 32-bit");
static_assert(!parse_fixed("3", 9).ok || sizeof(long) > 4, "single digit scaled overflow on 32-bit");
static_assert(parse_fixed("2.147483647", 9).v == 2147483647L, "32-bit limit fits");
static_assert(!parse_fixed("2.1474836475", 9).ok || sizeof(long) > 4, "fractional rounding overflow");
static_assert(!parse_fixed("1", -1).ok && !parse_fixed("1", 10).ok, "invalid decimal scale");

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
// a connection that dies before the status line is complete is a dropped connection, not malformed data
static_assert(http_parse("").st.err == Err::Truncated, "empty reply");
static_assert(http_parse("H").st.err == Err::Truncated, "cut in the protocol name");
static_assert(http_parse("HTTP/1.").st.err == Err::Truncated, "cut after the protocol name");
static_assert(http_parse("HTTP/1.1 20").st.err == Err::Truncated, "cut in the status code");
static_assert(http_parse("HX").st.err == Err::Parse, "short non-HTTP bytes are malformed, not truncated");
static_assert(http_parse("garbage").st.err == Err::Parse, "short garbage");
static_assert(http_parse("HTTP/1.1 503 x\r\n\r\n").code == 503, "status code");

static_assert(compass(0) == sv("N") && compass(90) == sv("E") && compass(138) == sv("SE") &&
    compass(350) == sv("N") && compass(-45) == sv("NW"), "compass");


// MET Norway: conversions, symbol mapping, and a whole parse, all evaluated by the compiler.
static_assert(c_to_f_t10(125) == 545 && c_to_f_t10(-30) == 266 && c_to_f_t10(0) == 320 && c_to_f_t10(-400) == -400, "C to F");
static_assert(ms_to_mph_t10(35) == 78 && ms_to_kmh_t10(35) == 126 && ms_to_mph_t10(0) == 0, "m/s conversions");
static_assert(met_symbol("clearsky_day").code == 0 && met_symbol("clearsky_day").is_day == 1, "symbol day");
static_assert(met_symbol("partlycloudy_night").code == 2 && met_symbol("partlycloudy_night").is_day == 0, "symbol night");
static_assert(met_symbol("heavyrainandthunder").code == 95 && met_symbol("heavyrainandthunder").is_day == -1, "symbol thunder");
static_assert(met_symbol("snow_polartwilight").code == 73 && met_symbol("snow_polartwilight").is_day == -1, "symbol twilight");
static_assert(met_symbol("bogus").code == -1 && met_symbol("").code == -1, "unknown symbol");
static_assert(json_first("[1,2]").value == sv("1") && json_first("[ ]").r == JsonFind::Absent &&
    json_first("[1,").r == JsonFind::Malformed && json_first("{}").r == JsonFind::Malformed, "json_first");

constexpr bool metno_parse_ok()
{
	constexpr sv doc = R"({"properties":{"timeseries":[{"time":"2026-10-24T06:00:00Z","data":{"instant":{"details":{"air_temperature":12.5,)"
	    R"("relative_humidity":76.7,"wind_from_direction":161.4,"wind_speed":3.5}},"next_1_hours":{"summary":{"symbol_code":"partlycloudy_night"}}}}]}})";
	Forecast f;
	Query q;
	Status s = metno_parse(doc, q, &f);
	return s.ok() && f.temp_t10 == 545 && f.wind_t10 == 78 && f.humidity == 77 && f.wind_dir == 161 && f.code == 2 && f.is_day == 0 &&
	    sv(f.time) == sv("2026-10-24T06:00") && !f.metric;
}
static_assert(metno_parse_ok(), "a whole MET Norway response parses at compile time");
constexpr bool metno_rejects()
{
	Forecast f;
	Query q;
	return metno_parse("{}", q, &f).err == Err::BadData && metno_parse(R"({"properties":{"timeseries":[]}})", q, &f).err == Err::BadData &&
	    metno_parse(R"({"properties":{"timeseries":[{"data":)", q, &f).err == Err::Parse;
}
static_assert(metno_rejects(), "bad MET Norway documents are refused");

} // namespace wx_static_tests
