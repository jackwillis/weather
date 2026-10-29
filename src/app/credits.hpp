// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// The one place credits and licence wording live, so the CLI, xbelike and the OS 9 About box cannot drift.
// Plain constant text: no heap, no formatting. Full licence texts are in LICENSE and THIRD_PARTY.md.
// Every line fits 46 columns, so a page fits the OS 9 About box (324 px) and xbelike's panel.
#pragma once
#include <cstddef>

namespace wx {

// Short form: printed by `belike -V` after the version line, and shown in About boxes.
inline constexpr const char *kCreditsShort[] = {
	"Copyright (c) 2026 Jack Willis",
	"License GPL-3.0-or-later, NO WARRANTY.",
	"Data: Open-Meteo.com, MET Norway (CC BY 4.0)",
	"NWS (NOAA): U.S. public-domain data",
};

struct CreditPage { const char *title; const char *const *lines; size_t n; };

inline constexpr const char *kCreditsWeather[] = {
	"Copyright (c) 2026 Jack Willis",
	"Free software under the GNU General",
	"Public License, version 3 or (at your",
	"option) any later version.",
	"There is NO WARRANTY.",
	"Source: github.com/jackwillis/weather",
	"Full notices: LICENSE, THIRD_PARTY.md",
};
inline constexpr const char *kCreditsData[] = {
	"Open-Meteo.com (CC BY 4.0)",
	"MET Norway Locationforecast (CC BY 4.0)",
	"Search: Open-Meteo / GeoNames (CC BY 4.0)",
	"creativecommons.org/licenses/by/4.0/",
	"Values are converted to your units",
	"and summarised.",
	"Weather stays free software, with no",
	"ads or subscriptions.",
};
inline constexpr const char *kCreditsNws[] = {
	"National Weather Service (NOAA)",
	"U.S. public-domain observations.",
	"weather.gov/disclaimer",
	"Values are converted and summarised.",
	"No NOAA/NWS endorsement or affiliation.",
};
inline constexpr const char *kCreditsSoftware[] = {
	"BearSSL by Thomas Pornin (MIT): TLS",
	"Mac OS 9 build, also:",
	"  Retro68 runtime (GPL-3.0+ with the",
	"  GCC runtime exception)",
	"  Multiversal Interfaces (MIT)",
};

// Full credits: `belike --credits`, and the Credits pages of the About boxes.
inline constexpr CreditPage kCreditPages[] = {
	{"Weather", kCreditsWeather, sizeof kCreditsWeather / sizeof *kCreditsWeather},
	{"Weather data", kCreditsData, sizeof kCreditsData / sizeof *kCreditsData},
	{"NWS observations", kCreditsNws, sizeof kCreditsNws / sizeof *kCreditsNws},
	{"Software", kCreditsSoftware, sizeof kCreditsSoftware / sizeof *kCreditsSoftware},
};
inline constexpr size_t kCreditPageCount = sizeof kCreditPages / sizeof *kCreditPages;

} // namespace wx
