// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Single-line text field editing (insert, delete, cursor). Printable ASCII only.
// Pure logic, shared by every UI; the UI only draws it and feeds it keys.
#pragma once
#include "core/text.hpp"

namespace wx {

template <size_t N>
struct TextEdit {
	char s[N] = {};
	size_t n = 0;     // length
	size_t cur = 0;   // cursor position, 0..n

	static constexpr bool printable(char c) { return c >= 32 && c <= 126; }

	void set(sv v)
	{
		n = 0;
		for (char c : v)
			if (printable(c) && n + 1 < N) s[n++] = c;
		s[n] = 0;
		cur = n;
	}
	bool insert(char c)
	{
		if (!printable(c) || n + 1 >= N) return false;
		for (size_t i = n + 1; i > cur; i--) s[i] = s[i - 1];
		s[cur++] = c;
		n++;
		return true;
	}
	bool backspace()
	{
		if (cur == 0) return false;
		for (size_t i = cur - 1; i < n; i++) s[i] = s[i + 1];
		cur--; n--;
		return true;
	}
	bool del()
	{
		if (cur >= n) return false;
		for (size_t i = cur; i < n; i++) s[i] = s[i + 1];
		n--;
		return true;
	}
	void left() { if (cur > 0) cur--; }
	void right() { if (cur < n) cur++; }
	void home() { cur = 0; }
	void end() { cur = n; }
	void set_cursor(size_t c) { cur = c > n ? n : c; }
	sv view() const { return sv(s, n); }
};

} // namespace wx
