// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Self-terminating, seed-free hardware acceptance for the pure compatibility helpers.
#include "platform/os9/portable.hpp"
#include "platform/os9/toolbox_extra.h"
#include <OSUtils.h>
#include <cstdio>
#include <cstring>
#include <climits>

int main()
{
	FILE *out = std::fopen("portable-probe.log", "w");
	if (!out) return 1;
	int failures = 0;
	char format[80], result[80];
	bool ok = wx::os9::rewrite_log_format("size=%08zu percent=%%", format, sizeof format);
	if (ok) std::snprintf(result, sizeof result, format, static_cast<size_t>(42));
	if (!ok || std::strcmp(result, "size=00000042 percent=%")) ++failures;
	std::fprintf(out, "format rewrite: %s\n", ok ? result : "FAIL");
	ok = wx::os9::decimal_signed(LLONG_MIN, result, sizeof result);
	if (!ok || std::strcmp(result, "-9223372036854775808")) ++failures;
	std::fprintf(out, "signed minimum: %s\n", result);
	ok = wx::os9::decimal_unsigned(ULLONG_MAX, result, sizeof result);
	if (!ok || std::strcmp(result, "18446744073709551615")) ++failures;
	std::fprintf(out, "unsigned maximum: %s\n", result);
	UInt32 mac; GetDateTime(&mac);
	MachineLocation location; ReadLocation(&location);
	wx::os9::decimal_signed(wx::os9::mac_to_unix(mac, location.u.gmtDelta), result, sizeof result);
	std::fprintf(out, "unix UTC: %s\n", result);
	std::fprintf(out, "gmt offset: %ld\n", wx::os9::gmt_offset(location.u.gmtDelta));
	std::fprintf(out, "failures: %d\n", failures);
	if (std::fclose(out)) return 1;
	return failures ? 1 : 0;
}
