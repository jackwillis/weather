// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// The one internal forecast shape every provider fills in, plus formatting.
#pragma once
#include "status.hpp"
#include "text.hpp"

namespace wx {

constexpr long kUnset = -999999;

struct Forecast {
	const char *source = "";
	bool saved_valid = false;   // runtime: reused before MET expiry, no request
	long long retrieved_at = 0; // runtime: original cache fetch time
	long long expires = 0;      // MET UTC expiry; zero means revalidate on next fetch
	char last_modified[64] = {}; // MET Norway response validator, never synthesized
	char time[24] = {};          // as the provider gave it, e.g. 2026-10-24T04:45
	bool metric = false;
	long temp_t10 = kUnset;      // tenths of C or F
	long humidity = kUnset;      // percent
	long wind_t10 = kUnset;      // tenths of km/h or mph
	long wind_dir = kUnset;      // degrees
	long code = kUnset;          // WMO weather code
	long is_day = kUnset;        // 0/1
};

struct WmoName { int code; const char *text; };
constexpr WmoName kWmo[] = {
	{0, "Clear sky"}, {1, "Mainly clear"}, {2, "Partly cloudy"}, {3, "Overcast"},
	{45, "Fog"}, {48, "Rime fog"},
	{51, "Light drizzle"}, {53, "Drizzle"}, {55, "Heavy drizzle"},
	{56, "Freezing drizzle"}, {57, "Heavy freezing drizzle"},
	{61, "Light rain"}, {63, "Rain"}, {65, "Heavy rain"},
	{66, "Freezing rain"}, {67, "Heavy freezing rain"},
	{71, "Light snow"}, {73, "Snow"}, {75, "Heavy snow"}, {77, "Snow grains"},
	{80, "Light showers"}, {81, "Showers"}, {82, "Violent showers"},
	{85, "Snow showers"}, {86, "Heavy snow showers"},
	{95, "Thunderstorm"}, {96, "Thunderstorm, hail"}, {99, "Severe thunderstorm, hail"},
};

constexpr const char *wmo_text(long code)
{
	for (const WmoName &w : kWmo)
		if (w.code == code) return w.text;
	return "Unknown conditions";
}

constexpr const char *compass(long deg)
{
	constexpr const char *names[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
	long d = ((deg % 360) + 360) % 360;
	return names[((d * 10 + 225) / 450) % 8];
}

constexpr bool plausible_temp_t10(long t10, bool metric)
{
	return metric ? (t10 >= -900 && t10 <= 600) : (t10 >= -1300 && t10 <= 1400);
}

// What a provider may claim. A value outside these limits means the answer is broken: refuse it, so the next source is
// tried, instead of displaying it. (Temperature is checked by plausible_temp_t10.)
constexpr Status check_fields(const Forecast &f, bool metric)
{
	if (f.humidity != kUnset && (f.humidity < 0 || f.humidity > 100)) return make(Err::BadData, "humidity out of range");
	if (f.wind_t10 != kUnset && (f.wind_t10 < 0 || f.wind_t10 > (metric ? 5000 : 3000))) return make(Err::BadData, "wind speed out of range");
	if (f.wind_dir != kUnset && (f.wind_dir < 0 || f.wind_dir > 360)) return make(Err::BadData, "wind direction out of range");
	if (f.is_day != kUnset && f.is_day != 0 && f.is_day != 1) return make(Err::BadData, "is_day is not 0 or 1");
	if (f.code < 0 || f.code > 99) return make(Err::BadData, "weather code out of range");
	return okay();
}

// Compact single line for scripts and status bars.
template <size_t N>
constexpr void format_oneline(Buf<N> &o, const Forecast &f)
{
	o.add_fixed(f.temp_t10, 1).add(f.metric ? "C " : "F ").add(wmo_text(f.code));
	if (f.wind_t10 != kUnset) {
		o.add(", wind ").add_fixed(f.wind_t10, 1).add(f.metric ? "km/h" : "mph");
		if (f.wind_dir != kUnset) o.add(' ').add(compass(f.wind_dir));
	}
	if (f.humidity != kUnset) o.add(", humidity ").add_int(f.humidity).add('%');
	o.add(" (").add(f.source).add(' ').add(f.time).add(')');
	if(f.saved_valid) o.add(" [saved; server-valid]");
}

template <size_t N>
constexpr void format_full(Buf<N> &o, const Forecast &f, sv place = sv())
{
	if(f.saved_valid) o.add("Saved forecast (valid until service expiry). ");
	o.add("Weather ");
	if (!place.empty()) o.add("for ").add(place).add(' ');
	o.add("from ").add(f.source).add(", forecast for ").add(f.time).add(" UTC\n  ");
	o.add_fixed(f.temp_t10, 1).add(f.metric ? " C   " : " F   ").add(wmo_text(f.code));
	if (f.is_day != kUnset) o.add(f.is_day ? " (day)" : " (night)");
	o.add('\n');
	if (f.wind_t10 != kUnset) {
		o.add("  Wind ").add_fixed(f.wind_t10, 1).add(f.metric ? " km/h" : " mph");
		if (f.wind_dir != kUnset) o.add(" from ").add(compass(f.wind_dir));
	}
	if (f.humidity != kUnset) o.add("   Humidity ").add_int(f.humidity).add('%');
	o.add('\n');
}

} // namespace wx
