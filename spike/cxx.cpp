// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#include <string_view>
#include <cstdio>
constexpr int digits(std::string_view s) { int n = 0; for (char c : s) if (c >= '0' && c <= '9') n++; return n; }
static_assert(digits("a1b22c333") == 6, "constexpr works");
int main() { std::printf("cxx17 ok %d\n", digits("x42")); return 0; }
