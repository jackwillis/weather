// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Compile-time tests of the portable core. If the core is wrong on a compiler, this does not compile there.
// Included by unittest.cpp (host and maul) and by the OS 9 probe (Retro68): a bad parser breaks the build, on every target.
#pragma once
#include "platform/os9/modal_clock.hpp"
#include "platform/os9/portable.hpp"
#include "core/config.hpp"
#include "core/forecast.hpp"
#include "core/http.hpp"
#include "core/json.hpp"
#include "core/metno.hpp"
#include "core/openmeteo.hpp"
#include "core/settings.hpp"
#include "app/locform.hpp"
#include "app/preferences.hpp"
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

// Line breaks: LF, CRLF and a lone CR; CRLF is one break, LF then CR is two.
static_assert(line_break("ab\ncd", 0) == 2 && line_break("ab\rcd", 0) == 2 && line_break("abcd", 0) == 4 && line_break("", 0) == 0 &&
    line_break("ab\ncd", 3) == 5, "line_break");
static_assert(after_break("ab\r\ncd", 2) == 4 && after_break("ab\rcd", 2) == 3 && after_break("ab\ncd", 2) == 3 &&
    after_break("ab\r", 2) == 3 && after_break("ab", 2) == 2 && after_break("a\r\r\nb", 1) == 2 && after_break("a\n\rb", 1) == 2,
    "after_break");

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

// Field ranges: a provider answer with an impossible humidity, wind, direction, day flag or code is refused (so the next source
// is tried), not shown. Boundaries are exact.
// Append t to doc (capacity cap, current length n); returns the new length. A plain function, not a lambda, for older compilers.
constexpr size_t append(char *doc, size_t n, size_t cap, sv t)
{
	for (char c : t) if (n + 1 < cap) doc[n++] = c;
	return n;
}
constexpr bool om_status(sv current_fields, bool metric, Err want, sv units = sv())
{
	char doc[600] = {};
	size_t n = append(doc, 0, sizeof doc, "{");
	if (!units.empty()) n = append(doc, append(doc, append(doc, n, sizeof doc, "\"current_units\":"), sizeof doc, units), sizeof doc, ",");
	n = append(doc, append(doc, n, sizeof doc, "\"current\":{\"temperature_2m\":20.0,\"weather_code\":0"), sizeof doc, current_fields);
	n = append(doc, n, sizeof doc, "}}");
	doc[n] = 0;
	Forecast f;
	Query q;
	q.metric = metric;
	return openmeteo_parse(sv(doc, n), q, &f).err == want;
}
static_assert(om_status("", false, Err::Ok), "baseline");
static_assert(om_status(",\"relative_humidity_2m\":0", false, Err::Ok) && om_status(",\"relative_humidity_2m\":100", false, Err::Ok), "humidity limits");
static_assert(om_status(",\"relative_humidity_2m\":101", false, Err::BadData) && om_status(",\"relative_humidity_2m\":-1", false, Err::BadData) &&
    om_status(",\"relative_humidity_2m\":850", false, Err::BadData), "humidity out of range");
static_assert(om_status(",\"wind_speed_10m\":0", false, Err::Ok) && om_status(",\"wind_speed_10m\":-12.3", false, Err::BadData) &&
    om_status(",\"wind_speed_10m\":300.1", false, Err::BadData) && om_status(",\"wind_speed_10m\":300", false, Err::Ok), "mph limits");
static_assert(om_status(",\"wind_speed_10m\":500", true, Err::Ok) && om_status(",\"wind_speed_10m\":500.1", true, Err::BadData), "km/h limits");
static_assert(om_status(",\"wind_direction_10m\":360", false, Err::Ok) && om_status(",\"wind_direction_10m\":0", false, Err::Ok) &&
    om_status(",\"wind_direction_10m\":361", false, Err::BadData) && om_status(",\"wind_direction_10m\":4000", false, Err::BadData) &&
    om_status(",\"wind_direction_10m\":-1", false, Err::BadData), "direction limits");
static_assert(om_status(",\"is_day\":1", false, Err::Ok) && om_status(",\"is_day\":7", false, Err::BadData) &&
    om_status(",\"is_day\":-1", false, Err::BadData), "is_day is 0 or 1");

// Units: when the response says what it is in, that must match what was asked for. The Open-Meteo spelling of mph is "mp/h".
constexpr sv kImpUnits = R"({"temperature_2m":"°F","wind_speed_10m":"mp/h"})";
constexpr sv kMetUnits = R"({"temperature_2m":"°C","wind_speed_10m":"km/h"})";
static_assert(om_status("", false, Err::Ok, kImpUnits) && om_status("", true, Err::Ok, kMetUnits), "matching units");
static_assert(om_status("", true, Err::BadData, kImpUnits) && om_status("", false, Err::BadData, kMetUnits), "units differ from the request");
static_assert(om_status("", false, Err::BadData, R"({"temperature_2m":"°F","wind_speed_10m":"m/s"})") &&
    om_status("", true, Err::BadData, R"({"temperature_2m":"°C","wind_speed_10m":"kn"})"), "unknown wind unit");
static_assert(om_status("", false, Err::Ok, R"({"temperature_2m":"°F"})") && om_status("", false, Err::Ok), "units absent: nothing to check");

// Explicit metadata is a typed contract; absent metadata remains compatible.
constexpr bool om_bad_unit_metadata()
{
	const sv bad[] = {"123", "null", "true", "[]", R"("units")",
	    R"({"temperature_2m":123})", R"({"temperature_2m":null})",
	    R"({"temperature_2m":false})", R"({"temperature_2m":{}})",
	    R"({"wind_speed_10m":123})", R"({"wind_speed_10m":null})",
	    R"({"wind_speed_10m":[]})", R"({"temperature_2m":"junkF"})",
	    R"({"temperature_2m":""})", R"({"wind_speed_10m":""})"};
	for (sv u : bad) if (!om_status("", false, Err::BadData, u)) return false;
	return true;
}
static_assert(om_bad_unit_metadata(), "malformed explicit unit metadata must be refused");
static_assert(om_status("", false, Err::Ok, "{}") &&
    om_status("", false, Err::Ok, R"({"temperature_2m":"F","wind_speed_10m":"mph"})") &&
    om_status("", true, Err::Ok, R"({"temperature_2m":"C","wind_speed_10m":"kmh"})"), "unit aliases and absent keys");

constexpr bool met_status(sv details, bool metric, Err want)
{
	char doc[400] = {};
	size_t n = append(doc, 0, sizeof doc, R"({"properties":{"timeseries":[{"time":"t","data":{"instant":{"details":{"air_temperature":5.0,"cloud_area_fraction":50.0)");
	n = append(doc, n, sizeof doc, details);
	n = append(doc, n, sizeof doc, R"(}}}}]}})");
	doc[n] = 0;
	Forecast f;
	Query q;
	q.metric = metric;
	return metno_parse(sv(doc, n), q, &f).err == want;
}
static_assert(met_status("", false, Err::Ok), "MET baseline");
static_assert(met_status(",\"relative_humidity\":100.0", false, Err::Ok) && met_status(",\"relative_humidity\":100.6", false, Err::BadData) &&
    met_status(",\"relative_humidity\":-0.6", false, Err::BadData) && met_status(",\"relative_humidity\":850", false, Err::BadData), "MET humidity");
static_assert(met_status(",\"wind_speed\":0.0", false, Err::Ok) && met_status(",\"wind_speed\":-3.0", false, Err::BadData) &&
    met_status(",\"wind_speed\":100.1", false, Err::BadData), "MET wind speed");
static_assert(met_status(",\"wind_from_direction\":359.9", false, Err::Ok) && met_status(",\"wind_from_direction\":4000", false, Err::BadData) &&
    met_status(",\"wind_from_direction\":-5", false, Err::BadData), "MET wind direction");
constexpr bool met_cloud_out_of_range()
{
	Forecast f;
	Query q;
	return metno_parse(R"({"properties":{"timeseries":[{"time":"t","data":{"instant":{"details":{"air_temperature":5.0,"cloud_area_fraction":150.0}}}}]}})", q, &f).err == Err::BadData;
}
static_assert(met_cloud_out_of_range(), "MET cloud cover above 100 %");
// On 32-bit targets (PowerPC, Retro68) long is 32 bits: these must be refused before they are multiplied by the unit factors.
static_assert(met_status(",\"wind_speed\":47721858.8", false, Err::BadData) && met_status(",\"wind_speed\":9700.0", true, Err::BadData), "MET wind overflow");
constexpr bool met_huge_temp()
{
	Forecast f;
	Query q;
	return metno_parse(R"({"properties":{"timeseries":[{"time":"t","data":{"instant":{"details":{"air_temperature":47721858.8,"cloud_area_fraction":50.0}}}}]}})", q, &f).err == Err::BadData;
}
static_assert(met_huge_temp(), "MET huge temperature is refused, not wrapped");

} // namespace wx_static_tests
