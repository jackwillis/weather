// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Separate diagnostic: restore the real cache, then fail before DNS or sockets.
#include "ui/xweather/run.hpp"
#include <cstdio>
using namespace wx;
static Status offline(Net &,const char *,const char *,int,Io *)
{
    puts("stale fixture: connect refused before DNS"); fflush(stdout);
    return make(Err::Tls,"deliberately long certificate failure that would hide an age appended after the error");
}
static void prepare(Net &net) { net.connect=offline; }
static void line(sv text)
{
    std::fputs("visible status: ",stdout);
    std::fwrite(text.data(),1,text.size(),stdout);
    std::putchar('\n'); std::fflush(stdout);
}
int main(int argc,char **argv)
{
    const XweatherTestHooks hooks{prepare,line};
    return run_xweather(argc,argv,nullptr,&hooks);
}
