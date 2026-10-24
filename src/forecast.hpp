// The one internal forecast shape every provider fills in, plus formatting.
#pragma once
#include "text.hpp"

namespace wx {

constexpr long kUnset = -999999;

struct Forecast {
	const char *source = "";
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
}

template <size_t N>
constexpr void format_full(Buf<N> &o, const Forecast &f)
{
	o.add("Weather from ").add(f.source).add(", observed ").add(f.time).add(" UTC\n  ");
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
