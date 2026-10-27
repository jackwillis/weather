// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Minimal JSON navigation: no allocation, no tree. Finds a key among the
// direct children of one object, so a same-named key in a sibling object
// (Open-Meteo's "current_units" repeats every key with a string) can never be
// mistaken for the real one.
#pragma once
#include "text.hpp"

namespace wx {

constexpr size_t kJsonMaxDepth = 16;

constexpr size_t json_ws(sv s, size_t i)
{
	while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) i++;
	return i;
}

constexpr size_t json_skip(sv s, size_t i, size_t depth = 0);

// i points at the opening quote; returns index after the closing quote.
constexpr size_t json_skip_string(sv s, size_t i)
{
	if (i >= s.size() || s[i] != '"') return sv::npos;
	for (i++; i < s.size(); i++) {
		if (s[i] == '\\') i++;
		else if (s[i] == '"') return i + 1;
	}
	return sv::npos;
}

// Skip one value starting at s[i] (after whitespace). Returns end index or npos.
constexpr size_t json_skip(sv s, size_t i, size_t depth)
{
	if (depth > kJsonMaxDepth) return sv::npos;
	i = json_ws(s, i);
	if (i >= s.size()) return sv::npos;
	char c = s[i];
	if (c == '"') return json_skip_string(s, i);
	if (c == '{' || c == '[') {
		char close = c == '{' ? '}' : ']';
		i = json_ws(s, i + 1);
		if (i < s.size() && s[i] == close) return i + 1;
		for (;;) {
			if (c == '{') {
				i = json_skip_string(s, json_ws(s, i));
				if (i == sv::npos) return sv::npos;
				i = json_ws(s, i);
				if (i >= s.size() || s[i] != ':') return sv::npos;
				i++;
			}
			i = json_skip(s, i, depth + 1);
			if (i == sv::npos) return sv::npos;
			i = json_ws(s, i);
			if (i >= s.size()) return sv::npos;
			if (s[i] == close) return i + 1;
			if (s[i] != ',') return sv::npos;
			i++;
		}
	}
	size_t st = i;  // number, true, false, null
	while (i < s.size() && s[i] != ',' && s[i] != '}' && s[i] != ']' &&
	    s[i] != ' ' && s[i] != '\t' && s[i] != '\r' && s[i] != '\n')
		i++;
	return i == st ? sv::npos : i;
}

enum class JsonFind { Found, Absent, Malformed };
struct JsonLookup {
	JsonFind r = JsonFind::Absent;
	sv value;
};

// Look up `key` among the direct children of the object `obj`.
constexpr JsonLookup json_get(sv obj, sv key)
{
	size_t i = json_ws(obj, 0);
	if (i >= obj.size() || obj[i] != '{') return {JsonFind::Malformed, {}};
	// The whole object must be well formed, so a cut-off document is never
	// half-trusted just because the key we want came before the cut.
	size_t end = json_skip(obj, i);
	if (end == sv::npos || json_ws(obj, end) != obj.size()) return {JsonFind::Malformed, {}};
	i = json_ws(obj, i + 1);
	if (i < obj.size() && obj[i] == '}') return {JsonFind::Absent, {}};
	for (;;) {
		size_t ks = json_ws(obj, i);
		size_t ke = json_skip_string(obj, ks);
		if (ke == sv::npos) return {JsonFind::Malformed, {}};
		sv k = obj.substr(ks + 1, ke - ks - 2);
		i = json_ws(obj, ke);
		if (i >= obj.size() || obj[i] != ':') return {JsonFind::Malformed, {}};
		size_t vs = json_ws(obj, i + 1);
		size_t ve = json_skip(obj, vs);
		if (ve == sv::npos) return {JsonFind::Malformed, {}};
		if (k == key) return {JsonFind::Found, obj.substr(vs, ve - vs)};
		i = json_ws(obj, ve);
		if (i >= obj.size()) return {JsonFind::Malformed, {}};
		if (obj[i] == '}') return {JsonFind::Absent, {}};
		if (obj[i] != ',') return {JsonFind::Malformed, {}};
		i++;
	}
}

// The first element of the array `arr` (the whole array must be well formed).
constexpr JsonLookup json_first(sv arr)
{
	size_t i = json_ws(arr, 0);
	if (i >= arr.size() || arr[i] != '[') return {JsonFind::Malformed, {}};
	size_t end = json_skip(arr, i);
	if (end == sv::npos || json_ws(arr, end) != arr.size()) return {JsonFind::Malformed, {}};
	i = json_ws(arr, i + 1);
	if (arr[i] == ']') return {JsonFind::Absent, {}};
	size_t ve = json_skip(arr, i);
	return {JsonFind::Found, arr.substr(i, ve - i)};
}

// Contents of a string value without quotes (escapes left as written).
constexpr bool json_str(sv v, sv *out)
{
	if (v.size() < 2 || v[0] != '"' || v[v.size() - 1] != '"') return false;
	*out = v.substr(1, v.size() - 2);
	return true;
}

// A bare JSON number, scaled by 10^decimals. Strings and literals are rejected.
constexpr Num json_num(sv v, int decimals)
{
	if (v.empty() || !((v[0] >= '0' && v[0] <= '9') || v[0] == '-')) return {};
	return parse_fixed(v, decimals);
}

} // namespace wx
