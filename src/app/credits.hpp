// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// The one place credits and licence wording live, so the CLI, xweather and the OS 9 About box cannot drift.
// Plain constant text: no heap, no formatting. Full licence texts are in LICENSE and THIRD_PARTY.md.
#pragma once

namespace wx {

// Short form: printed by `weather -V` after the version line, and shown in About boxes.
inline constexpr const char *kCreditsShort[] = {
	"Copyright (c) 2026 Jack Willis",
	"License GPL-3.0-or-later: free software, with NO WARRANTY.",
	"Weather data: Open-Meteo.com and MET Norway (CC BY 4.0).",
};

// Full credits: `weather --credits`, and the Credits page of the About boxes.
inline constexpr const char *kCreditsFull[] = {
	"Weather: copyright (c) 2026 Jack Willis.",
	"Free software under the GNU General Public License,",
	"version 3 or (at your option) any later version.",
	"There is NO WARRANTY.",
	"Source code: https://github.com/jackwillis/weather",
	"",
	"Weather data",
	"  Open-Meteo.com, CC BY 4.0 (https://open-meteo.com)",
	"  MET Norway Locationforecast, CC BY 4.0 (https://api.met.no)",
	"  Values are converted to your units and summarised.",
	"",
	"Software",
	"  BearSSL by Thomas Pornin (MIT) for TLS",
	"  Mac OS 9 build: Retro68 runtime by Wolfgang Thaller",
	"  and others (GPL-3.0+ with the GCC runtime exception),",
	"  Multiversal Interfaces (MIT)",
	"",
	"Full notices: LICENSE and THIRD_PARTY.md",
};

} // namespace wx
