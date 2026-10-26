// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// ~/.weatherrc: "key = value" lines, '#' comments. Command-line flags override it.
#pragma once
#include "log.hpp"
#include "status.hpp"
#include "text.hpp"

namespace wx {

struct Config {
	// Default location: Madison, WI 53703 (Capitol Square). Override in ~/.weatherrc or with -l.
	long lat_e4 = 430747;
	long lon_e4 = -893841;
	char name[48] = "Madison, WI";
	bool metric = false;
	bool auto_refresh = true;   // xweather: refresh every 15 minutes
	int timeout_s = 15;
	int attempts = 3;
	char providers[64] = "open-meteo";   // comma separated, in fallback order
	char contact[80] = "";               // e-mail or URL: sent in the User-Agent, required by some services
	Level level = LogWarn;
};

inline Status config_set(Config *c, sv key, sv val, int line)
{
	if (key == "latitude" || key == "longitude") {
		Num n = parse_fixed(val, 4);
		long lim = key == "latitude" ? 900000 : 1800000;
		if (!n.ok || n.v > lim || n.v < -lim) return make(Err::Config, "bad latitude/longitude", line);
		(key == "latitude" ? c->lat_e4 : c->lon_e4) = n.v;
	} else if (key == "name") {
		if (val.size() >= sizeof c->name) return make(Err::Config, "name too long (max 47)", line);
		for (size_t i = 0; i < val.size(); i++) {
			if (static_cast<unsigned char>(val[i]) < 32 || static_cast<unsigned char>(val[i]) > 126 || val[i] == '#')
				return make(Err::Config, "name must be printable ASCII without '#'", line);
			c->name[i] = val[i];
		}
		c->name[val.size()] = 0;
	} else if (key == "units") {
		if (val == "metric") c->metric = true;
		else if (val == "imperial") c->metric = false;
		else return make(Err::Config, "units must be metric or imperial", line);
	} else if (key == "autorefresh") {
		if (val == "on") c->auto_refresh = true;
		else if (val == "off") c->auto_refresh = false;
		else return make(Err::Config, "autorefresh must be on or off", line);
	} else if (key == "timeout" || key == "attempts") {
		Num n = parse_fixed(val, 0);
		if (!n.ok || n.v < 1 || n.v > 300) return make(Err::Config, "value must be 1..300", line);
		(key == "timeout" ? c->timeout_s : c->attempts) = static_cast<int>(n.v);
	} else if (key == "contact") {
		if (val.size() >= sizeof c->contact) return make(Err::Config, "contact too long (max 79)", line);
		for (size_t i = 0; i < val.size(); i++) {
			if (static_cast<unsigned char>(val[i]) < 32 || static_cast<unsigned char>(val[i]) > 126 || val[i] == '#' || val[i] == '(' || val[i] == ')')
				return make(Err::Config, "contact must be printable ASCII without '#', '(' or ')'", line);
			c->contact[i] = val[i];
		}
		c->contact[val.size()] = 0;
	} else if (key == "providers") {
		if (val.empty() || val.size() >= sizeof c->providers) return make(Err::Config, "bad providers list", line);
		for (size_t i = 0; i < val.size(); i++) c->providers[i] = val[i];
		c->providers[val.size()] = 0;
	} else if (key == "log") {
		if (val == "error") c->level = LogError;
		else if (val == "warn") c->level = LogWarn;
		else if (val == "info") c->level = LogInfo;
		else if (val == "debug") c->level = LogDebug;
		else return make(Err::Config, "log must be error, warn, info or debug", line);
	} else {
		return make(Err::Config, "unknown key", line);
	}
	return okay();
}

inline Status config_parse(sv text, Config *c)
{
	int line = 0;
	size_t pos = 0;
	while (pos <= text.size()) {
		size_t nl = line_break(text, pos);
		sv ln = text.substr(pos, nl - pos);
		line++;
		size_t hash = find(ln, "#");
		if (hash != sv::npos) ln = ln.substr(0, hash);
		ln = trim(ln);
		if (!ln.empty()) {
			size_t eq = find(ln, "=");
			if (eq == sv::npos) return make(Err::Config, "expected key = value", line);
			Status s = config_set(c, trim(ln.substr(0, eq)), trim(ln.substr(eq + 1)), line);
			if (!s.ok()) return s;
		}
		if (nl >= text.size()) break;
		pos = after_break(text, nl);
	}
	return okay();
}

} // namespace wx
