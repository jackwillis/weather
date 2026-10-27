// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Run on the host; the helpers themselves include no Toolbox headers.
#include "platform/os9/portable.hpp"
#include <climits>

static void test_os9_helpers()
{
	using namespace wx::os9;
	CHECK(mac_to_unix(2082844800U, 0) == 0);
	CHECK(mac_to_unix(0, 0) == -2082844800LL);
	CHECK(mac_to_unix(UINT32_MAX, 0) == 2212122495LL);
	CHECK(gmt_offset(0x00ffffffU) == -1);
	CHECK(gmt_offset(0x00800000U) == -8388608);
	CHECK(gmt_offset(0x007fffffU) == 8388607);
	CHECK(mac_to_unix(2082844800U, 19800) == -19800);
	CHECK(mac_to_unix(2082844800U, 0x00ffb9b0U) == 18000);
	CHECK(mac_to_unix(2082844800U, 0x80ffb9b0U) == 18000); // DST already in offset.
	CHECK(mac_to_unix(2082844800U, 0x7fffb9b0U) == 18000); // Ignore entire high byte.
	char out[128];
	CHECK(decimal_signed(LLONG_MIN,out,sizeof out) && !strcmp(out,"-9223372036854775808"));
	CHECK(decimal_signed(LLONG_MAX,out,sizeof out) && !strcmp(out,"9223372036854775807"));
	CHECK(decimal_unsigned(ULLONG_MAX,out,sizeof out) && !strcmp(out,"18446744073709551615"));
	CHECK(decimal_signed(0,out,sizeof out) && !strcmp(out,"0"));
	CHECK(decimal_signed(-1,out,3) && !strcmp(out,"-1"));
	CHECK(!decimal_signed(-1,out,2) && out[0]==0);
	CHECK(!decimal_unsigned(1,nullptr,0));
	CHECK(!decimal_unsigned(1,out,1) && out[0]==0);
	CHECK(rewrite_log_format("%zu %zd %zx %%zu %08zu %.*zu %lld",out,sizeof out));
	CHECK(!strcmp(out,"%lu %ld %lx %%zu %08lu %.*lu %lld"));
	CHECK(rewrite_log_format("%% %s %ld %.2f",out,sizeof out));
	CHECK(!strcmp(out,"%% %s %ld %.2f"));
	CHECK(rewrite_log_format("abc",out,4) && !strcmp(out,"abc"));
	CHECK(!rewrite_log_format("abc",out,3) && out[0]==0);
	CHECK(!rewrite_log_format("%",out,sizeof out) && out[0]==0);
	CHECK(!rewrite_log_format("%0",out,sizeof out) && out[0]==0);
	CHECK(!rewrite_log_format(wx::sv("a\0b",3),out,sizeof out) && out[0]==0);
	CHECK(!rewrite_log_format("",nullptr,0));
}
