// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// MET Norway Locationforecast 2.0 "compact" (https://api.met.no): worldwide, no key, one request.
// The first time step is the current hour. Their terms require a User-Agent with contact details,
// so this provider is marked needs_contact. The service answers in metric units; we convert.
#pragma once
#include "json.hpp"
#include "provider.hpp"

namespace wx {

struct MetSymbol { const char *name; int code; };
// symbol_code (without the _day/_night/_polartwilight suffix) -> WMO weather code
constexpr MetSymbol kMetSymbols[] = {
	{"clearsky", 0}, {"fair", 1}, {"partlycloudy", 2}, {"cloudy", 3}, {"fog", 45},
	{"lightrain", 61}, {"rain", 63}, {"heavyrain", 65},
	{"lightrainshowers", 80}, {"rainshowers", 81}, {"heavyrainshowers", 82},
	{"lightsleet", 66}, {"sleet", 66}, {"heavysleet", 67},
	{"lightsleetshowers", 66}, {"sleetshowers", 66}, {"heavysleetshowers", 67},
	{"lightsnow", 71}, {"snow", 73}, {"heavysnow", 75},
	{"lightsnowshowers", 85}, {"snowshowers", 85}, {"heavysnowshowers", 86},
	{"lightrainandthunder", 95}, {"rainandthunder", 95}, {"heavyrainandthunder", 95},
	{"lightrainshowersandthunder", 95}, {"rainshowersandthunder", 95}, {"heavyrainshowersandthunder", 95},
	{"lightsleetandthunder", 95}, {"sleetandthunder", 95}, {"heavysleetandthunder", 95},
	// Locationforecast 2.0 retains these upstream misspellings for compatibility.
	{"lightssleetshowersandthunder", 95}, {"lightssnowshowersandthunder", 95},
	{"lightsleetshowersandthunder", 95}, {"sleetshowersandthunder", 95}, {"heavysleetshowersandthunder", 95},
	{"lightsnowandthunder", 95}, {"snowandthunder", 95}, {"heavysnowandthunder", 95},
	{"lightsnowshowersandthunder", 95}, {"snowshowersandthunder", 95}, {"heavysnowshowersandthunder", 95},
};

constexpr bool ends_with(sv s, sv suffix) { return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix; }

struct MetCode { int code; int is_day; };   // code -1 = unknown symbol; is_day kUnset-like (-1) when the symbol has no day/night
constexpr MetCode met_symbol(sv sym)
{
	int day = -1;
	if (ends_with(sym, "_day")) { day = 1; sym = sym.substr(0, sym.size() - 4); }
	else if (ends_with(sym, "_night")) { day = 0; sym = sym.substr(0, sym.size() - 6); }
	else if (ends_with(sym, "_polartwilight")) { sym = sym.substr(0, sym.size() - 14); }
	for (const MetSymbol &m : kMetSymbols)
		if (sym == sv(m.name)) return {m.code, day};
	return {-1, day};
}

// Tenths of degrees Celsius -> Fahrenheit, rounded half away from zero.
constexpr long c_to_f_t10(long c10) { return (c10 * 9 + (c10 >= 0 ? 2 : -2)) / 5 + 320; }
// Tenths of m/s -> tenths of km/h and mph, rounded.
constexpr long ms_to_kmh_t10(long ms10) { return (ms10 * 36 + 5) / 10; }
constexpr long ms_to_mph_t10(long ms10) { return (ms10 * 22369 + 5000) / 10000; }

constexpr bool metno_path(const Query &q, char *out, size_t cap)
{
	Buf<160> b;
	b.add("/weatherapi/locationforecast/2.0/compact?lat=").add_fixed(q.lat_e4, 4).add("&lon=").add_fixed(q.lon_e4, 4);
	if (b.truncated || b.n + 1 > cap) return false;
	for (size_t i = 0; i <= b.n; i++) out[i] = b.s[i];
	return true;
}
inline bool metno_path_fn(const Query &q, char *out, size_t cap) { return metno_path(q, out, cap); }

// descend: obj.key must exist and be found
constexpr Status met_get(sv obj, sv key, sv *out, const char *msg)
{
	JsonLookup l = json_get(obj, key);
	if (l.r == JsonFind::Malformed) return make(Err::Parse, "malformed JSON");
	if (l.r == JsonFind::Absent) return make(Err::BadData, msg);
	*out = l.value;
	return okay();
}

constexpr Status metno_parse(sv body, const Query &q, Forecast *f)
{
	sv props, series, first, data, inst, details;
	Status s = met_get(body, "properties", &props, "no 'properties' in response");
	if (!s.ok()) return s;
	s = met_get(props, "timeseries", &series, "no 'timeseries' in response");
	if (!s.ok()) return s;
	JsonLookup fl = json_first(series);
	if (fl.r == JsonFind::Malformed) return make(Err::Parse, "malformed JSON");
	if (fl.r == JsonFind::Absent) return make(Err::BadData, "empty timeseries");
	first = fl.value;
	s = met_get(first, "data", &data, "no 'data' in the first time step");
	if (!s.ok()) return s;
	s = met_get(data, "instant", &inst, "no 'instant' data");
	if (!s.ok()) return s;
	s = met_get(inst, "details", &details, "no 'details' in the instant data");
	if (!s.ok()) return s;

	long c10 = 0, ms10 = 0, rh10 = 0, dir10 = 0, cloud10 = 0;
	bool have_wind = false, have_rh = false, have_dir = false, have_cloud = false;
	JsonLookup t = json_get(details, "air_temperature");
	if (t.r != JsonFind::Found) return make(Err::BadData, "air_temperature missing");
	Num n = json_num(t.value, 1);
	if (!n.ok) return make(Err::BadData, "air_temperature is not a number");
	c10 = n.v;
	JsonLookup w = json_get(details, "wind_speed");
	if (w.r == JsonFind::Found) { n = json_num(w.value, 1); if (n.ok) { ms10 = n.v; have_wind = true; } }
	JsonLookup h = json_get(details, "relative_humidity");
	if (h.r == JsonFind::Found) { n = json_num(h.value, 1); if (n.ok) { rh10 = n.v; have_rh = true; } }
	JsonLookup d = json_get(details, "wind_from_direction");
	if (d.r == JsonFind::Found) { n = json_num(d.value, 1); if (n.ok) { dir10 = n.v; have_dir = true; } }
	JsonLookup cl = json_get(details, "cloud_area_fraction");
	if (cl.r == JsonFind::Found) { n = json_num(cl.value, 1); if (n.ok) { cloud10 = n.v; have_cloud = true; } }

	// Raw values are range-checked before they are converted: on 32-bit targets (PowerPC, Retro68) long is 32 bits and the
	// unit factors would overflow, and rounding must not turn -0.6 % into 0 %. The metric range holds for the Fahrenheit result too.
	if (!plausible_temp_t10(c10, true)) return make(Err::BadData, "temperature out of plausible range");
	if (have_wind && (ms10 < 0 || ms10 > 1000)) return make(Err::BadData, "wind speed out of range");
	if (have_rh && (rh10 < 0 || rh10 > 1000)) return make(Err::BadData, "humidity out of range");
	if (have_dir && (dir10 < 0 || dir10 > 3600)) return make(Err::BadData, "wind direction out of range");
	if (have_cloud && (cloud10 < 0 || cloud10 > 1000)) return make(Err::BadData, "cloud cover out of range");
	long temp = q.metric ? c10 : c_to_f_t10(c10);

	// the symbol for the next hour describes "now" best
	MetCode mc{-1, -1};
	JsonLookup n1 = json_get(data, "next_1_hours");
	if (n1.r == JsonFind::Found) {
		JsonLookup sm = json_get(n1.value, "summary");
		if (sm.r == JsonFind::Found) {
			JsonLookup sc = json_get(sm.value, "symbol_code");
			sv name;
			if (sc.r == JsonFind::Found && json_str(sc.value, &name)) mc = met_symbol(name);
		}
	}
	long code = mc.code;
	if (code < 0) {                                   // no usable symbol: estimate from cloud cover
		if (!have_cloud) return make(Err::BadData, "no weather symbol and no cloud cover");
		code = cloud10 < 200 ? 0 : cloud10 < 450 ? 1 : cloud10 < 750 ? 2 : 3;
	}

	f->temp_t10 = temp;
	f->code = code;
	f->is_day = mc.is_day >= 0 ? mc.is_day : kUnset;
	if (have_wind) f->wind_t10 = q.metric ? ms_to_kmh_t10(ms10) : ms_to_mph_t10(ms10);
	if (have_rh) f->humidity = (rh10 + 5) / 10;
	if (have_dir) f->wind_dir = (dir10 + 5) / 10 % 360;
	JsonLookup tm = json_get(first, "time");
	sv ts;
	if (tm.r == JsonFind::Found && json_str(tm.value, &ts)) {
		size_t k = ts.size() < 16 ? ts.size() : 16;      // "2026-10-24T06:00:00Z" -> "2026-10-24T06:00"
		for (size_t i = 0; i < k; i++) f->time[i] = ts[i];
		f->time[k] = 0;
	}
	f->source = "met-no";
	f->metric = q.metric;
	return check_fields(*f, q.metric);
}

constexpr Provider kMetNo = {
	"met-no", "api.met.no", "443", metno_path_fn, metno_parse, true,
};

} // namespace wx
