// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// OS 9 probe: does Retro68's compiler accept the portable core, and do all compile-time tests pass on it?
// Compile-only (and link) test; nothing here runs.
#include "../test/static_tests.hpp"
#include "app/model.hpp"
#include "core/fetch.hpp"

int main()
{
	wx::Model m;
	wx::Buf<64> b;
	m.status_line(b, 0);
	return b.n > 0 ? 0 : 1;
}
