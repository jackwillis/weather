// Compile-time tests of the portable core. If the core is wrong on a compiler, this does not compile there.
// Included by unittest.cpp (host and maul) and by the OS 9 probe (Retro68): a bad parser breaks the build, on every target.
#pragma once
#include "core/config.hpp"
#include "core/forecast.hpp"
#include "core/http.hpp"
#include "core/json.hpp"
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


} // namespace wx_static_tests
