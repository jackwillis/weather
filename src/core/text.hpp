// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Fixed-capacity string builder and number helpers. No heap, all constexpr.
#pragma once
#include <cstddef>
#include <string_view>
#include <limits>

namespace wx {

using sv = std::string_view;

template <size_t N>
struct Buf {
	char s[N] = {};
	size_t n = 0;
	bool truncated = false;

	constexpr void put(char c)
	{
		if (n + 1 < N) { s[n++] = c; s[n] = 0; }
		else truncated = true;
	}
	constexpr Buf &add(sv v) { for (char c : v) put(c); return *this; }
	constexpr Buf &add(char c) { put(c); return *this; }
	constexpr Buf &add_int(long v)
	{
		char t[24] = {};
		int k = 0;
		bool neg = v < 0;
		unsigned long u = neg ? 0UL - static_cast<unsigned long>(v) : static_cast<unsigned long>(v);
		do { t[k++] = static_cast<char>('0' + u % 10); u /= 10; } while (u);
		if (neg) put('-');
		while (k) put(t[--k]);
		return *this;
	}
	// value is scaled by 10^decimals: add_fixed(548, 1) -> "54.8"
	constexpr Buf &add_fixed(long v, int decimals)
	{
		unsigned long u = v < 0 ? 0UL - static_cast<unsigned long>(v) : static_cast<unsigned long>(v);
		unsigned long div = 1;
		for (int i = 0; i < decimals; i++) div *= 10;
		if (v < 0) put('-');
		add_int(static_cast<long>(u / div));
		if (decimals > 0) {
			put('.');
			unsigned long frac = u % div;
			for (unsigned long d = div / 10; d >= 1; d /= 10) {
				put(static_cast<char>('0' + frac / d % 10));
				if (d == 1) break;
			}
		}
		return *this;
	}
	constexpr sv view() const { return sv(s, n); }
	constexpr const char *c_str() const { return s; }
};

struct Num {
	bool ok = false;
	long v = 0;
};

// Parse "-12.345" into a long scaled by 10^decimals, rounding half away from
// zero. No exponents, no whitespace, at most 9 integer digits.
constexpr Num parse_fixed(sv t, int decimals)
{
	size_t i = 0;
	bool neg = false;
	if (i < t.size() && (t[i] == '-' || t[i] == '+')) { neg = t[i] == '-'; i++; }
	// Check arithmetic before it happens: long is only 32 bits on macppc.
	if (decimals < 0 || decimals > 9) return {};
	long scale = 1;
	for (int k = 0; k < decimals; k++) scale *= 10;
	const long max = std::numeric_limits<long>::max();
	long ip = 0;
	int idig = 0;
	while (i < t.size() && t[i] >= '0' && t[i] <= '9') {
		if (++idig > 9) return {};
		int digit = t[i] - '0';
		const long limit = max / scale;
		if (ip > limit / 10 || (ip == limit / 10 && digit > limit % 10)) return {};
		ip = ip * 10 + digit;
		i++;
	}
	long v = ip * scale;
	int fdig = 0;
	if (i < t.size() && t[i] == '.') {
		i++;
		long place = scale;
		bool round_up = false;
		while (i < t.size() && t[i] >= '0' && t[i] <= '9') {
			if (fdig < decimals) { place /= 10; long digit = (t[i] - '0') * place; if (v > max - digit) return {}; v += digit; }
			else if (fdig == decimals) round_up = t[i] >= '5';
			fdig++;
			i++;
		}
		if (round_up) { if (v == max) return {}; v += 1; }
		if (fdig == 0) return {};
	}
	if (i != t.size() || idig == 0) return {};
	return {true, neg ? -v : v};
}

constexpr bool eq_nocase(sv a, sv b)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); i++) {
		char x = a[i], y = b[i];
		if (x >= 'A' && x <= 'Z') x = static_cast<char>(x + 32);
		if (y >= 'A' && y <= 'Z') y = static_cast<char>(y + 32);
		if (x != y) return false;
	}
	return true;
}

constexpr sv trim(sv t)
{
	size_t b = 0, e = t.size();
	while (b < e && (t[b] == ' ' || t[b] == '\t' || t[b] == '\r' || t[b] == '\n')) b++;
	while (e > b && (t[e - 1] == ' ' || t[e - 1] == '\t' || t[e - 1] == '\r' || t[e - 1] == '\n')) e--;
	return t.substr(b, e - b);
}

// Text files may end lines with LF, CRLF or a lone CR (classic Mac OS). line_break: index of the first break at or after
// pos, or t.size(). after_break: index just past the break at `at` (CRLF counts as one); t.size() if there is none.
constexpr size_t line_break(sv t, size_t pos)
{
	while (pos < t.size() && t[pos] != '\n' && t[pos] != '\r') pos++;
	return pos;
}
constexpr size_t after_break(sv t, size_t at)
{
	if (at >= t.size()) return t.size();
	return (t[at] == '\r' && at + 1 < t.size() && t[at + 1] == '\n') ? at + 2 : at + 1;
}

constexpr size_t find(sv h, sv needle, size_t from = 0)
{
	if (needle.size() == 0) return from;
	for (size_t i = from; i + needle.size() <= h.size(); i++) {
		size_t k = 0;
		while (k < needle.size() && h[i + k] == needle[k]) k++;
		if (k == needle.size()) return i;
	}
	return sv::npos;
}

} // namespace wx
