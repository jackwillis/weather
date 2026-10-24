// Open-Meteo (https://open-meteo.com), "current" conditions, no API key.
#pragma once
#include "json.hpp"
#include "provider.hpp"

namespace wx {

constexpr bool openmeteo_path_buf(const Query &q, char *out, size_t cap)
{
	Buf<256> b;
	b.add("/v1/forecast?latitude=").add_fixed(q.lat_e4, 4)
	    .add("&longitude=").add_fixed(q.lon_e4, 4)
	    .add("&current=temperature_2m,relative_humidity_2m,is_day,weather_code,"
	         "wind_speed_10m,wind_direction_10m");
	b.add(q.metric ? "&temperature_unit=celsius&wind_speed_unit=kmh"
	               : "&temperature_unit=fahrenheit&wind_speed_unit=mph");
	if (b.truncated || b.n + 1 > cap) return false;
	for (size_t i = 0; i <= b.n; i++) out[i] = b.s[i];
	return true;
}

// Required numeric field of the "current" object, scaled by 10^dec.
constexpr Status om_field(sv cur, sv key, int dec, bool required, long *out, const char *msg)
{
	JsonLookup l = json_get(cur, key);
	if (l.r == JsonFind::Malformed) return make(Err::Parse, "malformed JSON in 'current'");
	if (l.r == JsonFind::Absent) return required ? make(Err::BadData, msg) : okay();
	Num n = json_num(l.value, dec);
	if (!n.ok) return required ? make(Err::BadData, msg) : okay();
	*out = n.v;
	return okay();
}

constexpr Status openmeteo_parse(sv body, const Query &q, Forecast *f)
{
	JsonLookup root_err = json_get(body, "error");
	if (root_err.r == JsonFind::Found && root_err.value == sv("true")) {
		return make(Err::BadData, "Open-Meteo reported an error");
	}
	JsonLookup cur = json_get(body, "current");
	if (cur.r == JsonFind::Malformed) return make(Err::Parse, "malformed JSON");
	if (cur.r == JsonFind::Absent) return make(Err::BadData, "no 'current' object in response");

	Status s = om_field(cur.value, "temperature_2m", 1, true, &f->temp_t10,
	    "temperature_2m missing or not a number");
	if (!s.ok()) return s;
	s = om_field(cur.value, "weather_code", 0, true, &f->code, "weather_code missing or not a number");
	if (!s.ok()) return s;
	s = om_field(cur.value, "relative_humidity_2m", 0, false, &f->humidity, "");
	if (!s.ok()) return s;
	s = om_field(cur.value, "wind_speed_10m", 1, false, &f->wind_t10, "");
	if (!s.ok()) return s;
	s = om_field(cur.value, "wind_direction_10m", 0, false, &f->wind_dir, "");
	if (!s.ok()) return s;
	s = om_field(cur.value, "is_day", 0, false, &f->is_day, "");
	if (!s.ok()) return s;

	if (!plausible_temp_t10(f->temp_t10, q.metric))
		return make(Err::BadData, "temperature out of plausible range");
	JsonLookup t = json_get(cur.value, "time");
	sv ts;
	if (t.r == JsonFind::Found && json_str(t.value, &ts)) {
		size_t n = ts.size() < sizeof f->time - 1 ? ts.size() : sizeof f->time - 1;
		for (size_t i = 0; i < n; i++) f->time[i] = ts[i];
		f->time[n] = 0;
	}
	f->source = "open-meteo";
	f->metric = q.metric;
	return okay();
}

inline bool openmeteo_path(const Query &q, char *out, size_t cap) { return openmeteo_path_buf(q, out, cap); }

constexpr Provider kOpenMeteo = {
	"open-meteo", "api.open-meteo.com", "443", openmeteo_path, openmeteo_parse,
};

} // namespace wx
