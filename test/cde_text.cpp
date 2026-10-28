// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Xlib font metrics need no server for these bounded text helpers.
#include "ui/xweather/cde.hpp"
#include <cstdio>
#include <cstring>
int main()
{
    int failures=0, checks=0;
    auto check=[&](bool value) { ++checks; if(!value) { ++failures; std::printf("FAIL CDE text check %d\n",checks); } };
    XFontStruct font{}; font.min_char_or_byte2=0; font.max_char_or_byte2=255;
    font.min_bounds.width=font.max_bounds.width=7; font.all_chars_exist=True;
    char out[32];
    check(cde::fit_text(&font,wx::sv{},100,out,sizeof out)==0 && out[0]==0);
    check(cde::text_width(nullptr,wx::sv{})==0);
    cde::Theme theme; cde::text(theme,0,nullptr,0,0,wx::sv{},0);
    check(cde::fit_text(&font,"abcdef",20,nullptr,0)==0);
    check(cde::fit_text(&font,"abcdef",28,out,sizeof out)==4 && !std::strcmp(out,"a..."));
    check(cde::fit_text(&font,"abc",21,out,sizeof out)==3 && !std::strcmp(out,"abc"));
    for(size_t cap=0;cap<=8;cap++) for(int width:{0,7,14,21,28}) {
        char guarded[16]; std::memset(guarded,'Z',sizeof guarded);
        size_t n=cde::fit_text(&font,"abcdefghij",width,guarded+4,cap);
        check(cap?n<cap && guarded[4+n]==0:n==0);
        check(guarded[3]=='Z' && guarded[4+cap]=='Z');
    }
    std::printf("CDE text: %d checks, %d failed\n",checks,failures);
    return failures?1:0;
}
