// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Measurement only: no networking, seed files, key contents or entropy credit.
#include "platform/os9/toolbox_extra.h"
#include "platform/os9/portable.hpp"
#include <Events.h>
#include <Files.h>
#include <Fonts.h>
#include <Menus.h>
#include <OSUtils.h>
#include <Processes.h>
#include <Quickdraw.h>
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {
constexpr unsigned kCapacity=8192;
constexpr uint32_t kDuration=90*60, kIdle=30*60;
constexpr uint64_t kInvalid=UINT64_MAX;
struct Sample {
    uint64_t us, call, wait, delay, disk;
    uint32_t ticks, event_ticks;
    short x,y,kind;
    unsigned short phase;
};
Sample samples[kCapacity];
WindowPtr window;
bool foreground=true, quit=false;
uint32_t active_ticks=0;
short app_volume=0; long app_directory=0;

uint64_t micros() {
    UnsignedWide value; Microseconds(&value);
    return (static_cast<uint64_t>(value.hi)<<32)|value.lo;
}
uint64_t elapsed(uint64_t a,uint64_t b) { return b>=a?b-a:kInvalid; }
const unsigned char *ptext(const char *s) {
    static unsigned char value[256]; size_t n=std::strlen(s); if(n>255) n=255;
    value[0]=static_cast<unsigned char>(n); std::memcpy(value+1,s,n); return value;
}
void text(short y,const char *s) { MoveTo(16,y); DrawString(ptext(s)); }
void draw() {
    SetPort(window); Rect bounds={0,0,165,460}; EraseRect(&bounds); TextSize(12);
    text(25,"Timing measurement only - no seed is created.");
    text(50,active_ticks<kIdle?"First 30 seconds: leave mouse and keyboard idle.":"Next 60 seconds: move mouse and type in this window.");
    text(75,"Key contents are never recorded. Close or Cmd-Q cancels.");
    text(100,"Sampling pauses outside this app. No other files are read.");
    char progress[80]; std::snprintf(progress,sizeof progress,"Active seconds: %lu of 90",static_cast<unsigned long>(active_ticks/60));
    text(130,progress);
}
void event(const EventRecord &e) {
    if(e.what==updateEvt) {
        WindowPtr target=reinterpret_cast<WindowPtr>(e.message);
        BeginUpdate(target); if(target==window) draw(); EndUpdate(target);
    } else if(e.what==osEvt && (static_cast<unsigned long>(e.message)>>24)==1) {
        foreground=(e.message&1)!=0;
    } else if(e.what==keyDown && (e.modifiers&cmdKey) && (e.message&255)=='q') quit=true;
    else if(e.what==mouseDown) {
        WindowPtr target=nullptr;
        short part=FindWindow(e.where,&target);
        if(part==inGoAway && target==window && TrackGoAway(window,e.where)) quit=true;
        else if(target==window) SelectWindow(window);
    }
}
bool spec(const char *name,FSSpec &out) { return FSMakeFSSpec(app_volume,app_directory,ptext(name),&out)==fnfErr; }
bool write_bytes(short fd,const char *bytes) {
    long wanted=static_cast<long>(std::strlen(bytes)), count=wanted;
    return FSWrite(fd,&count,bytes)==noErr && count==wanted;
}
bool save(short fd,unsigned count,bool complete) {
    if(!write_bytes(fd,"# WeatherEntropyProbe v1\nindex,phase,ticks,us,call_us,wait_us,delay_us,disk_us,event_kind,event_ticks,mouse_x,mouse_y\n")) return false;
    for(unsigned i=0;i<count;i++) {
        const Sample &s=samples[i]; char values[5][21];
        uint64_t raw[]={s.us,s.call,s.wait,s.delay,s.disk};
        for(unsigned n=0;n<5;n++) if(!wx::os9::decimal_unsigned(raw[n],values[n],sizeof values[n])) return false;
        char line[256];
        int n=std::snprintf(line,sizeof line,"%lu,%u,%lu,%s,%s,%s,%s,%s,%d,%lu,%d,%d\n",
            static_cast<unsigned long>(i),s.phase,static_cast<unsigned long>(s.ticks),
            values[0],values[1],values[2],values[3],values[4],s.kind,
            static_cast<unsigned long>(s.event_ticks),s.x,s.y);
        if(n<0 || n>=static_cast<int>(sizeof line) || !write_bytes(fd,line)) return false;
    }
    char footer[80]; std::snprintf(footer,sizeof footer,"# complete=%d samples=%lu\n",complete?1:0,static_cast<unsigned long>(count));
    return write_bytes(fd,footer);
}
}
int main() {
    InitGraf(&qd.thePort); InitFonts(); InitWindows(); InitMenus(); InitCursor();
    ProcessSerialNumber psn;
    if(GetCurrentProcess(&psn)!=noErr) return 2;
    FSSpec app; ProcessInfoRec info={};
    info.processInfoLength=sizeof info; info.processAppSpec=&app;
    if(GetProcessInformation(&psn,&info)!=noErr) return 2;
    app_volume=app.vRefNum; app_directory=app.parID;
    char name[32],input_name[32]; unsigned long start=TickCount();
    std::snprintf(name,sizeof name,"Entropy-%lx-%lx.csv",static_cast<unsigned long>(psn.lowLongOfPSN),start);
    std::snprintf(input_name,sizeof input_name,"EP-input-%lx-%lx",static_cast<unsigned long>(psn.lowLongOfPSN),start);
    FSSpec output,input;
    if(!spec(name,output) || !spec(input_name,input)) return 2; // never replace a prior file
    if(FSpCreate(&output,'Wthr','TEXT',0)!=noErr) return 2;
    short outfd=0,infd=0;
    if(FSpOpenDF(&output,fsWrPerm,&outfd)!=noErr) return 2;
    if(FSpCreate(&input,'Wthr','BINA',0)!=noErr) { FSClose(outfd); return 2; }
    bool ready=FSpOpenDF(&input,fsRdWrPerm,&infd)==noErr;
    unsigned char public_data[512]={}; long size=sizeof public_data;
    if(ready) ready=FSWrite(infd,&size,public_data)==noErr && size==sizeof public_data;
    if(ready) ready=FlushVol(nullptr,input.vRefNum)==noErr;
    Rect bounds={70,70,235,530};
    window=ready?NewWindow(nullptr,&bounds,ptext("Weather timing probe"),true,documentProc,reinterpret_cast<WindowPtr>(-1),true,0):nullptr;
    unsigned count=0;
    if(window) {
        SetPort(window); draw(); uint32_t drawn=UINT32_MAX;
        while(!quit && active_ticks<kDuration && count<kCapacity) {
            uint32_t begin=static_cast<uint32_t>(TickCount()); bool was_foreground=foreground;
            uint64_t a=micros(),b=micros();
            EventRecord e={}; bool got=WaitNextEvent(everyEvent,&e,1,nullptr);
            uint64_t c=micros(); if(got) event(e);
            if(quit) break;
            if(!foreground || !was_foreground || FrontWindow()!=window) { long final; Delay(1,&final); continue; }
            Point mouse; GetMouse(&mouse);
            uint64_t d=micros(); long final; Delay(1,&final); uint64_t end_delay=micros();
            long wanted=sizeof public_data;
            OSErr error=SetFPos(infd,fsFromStart,0);
            if(!error) error=FSRead(infd,&wanted,public_data);
            uint64_t end=micros(); uint32_t ticks=static_cast<uint32_t>(TickCount());
            samples[count++]={end,elapsed(a,b),elapsed(b,c),elapsed(d,end_delay),
                !error && wanted==sizeof public_data?elapsed(end_delay,end):kInvalid,
                ticks,static_cast<uint32_t>(got?e.when:ticks),mouse.h,mouse.v,static_cast<short>(got?e.what:0),
                static_cast<unsigned short>(active_ticks<kIdle?0:1)};
            active_ticks+=static_cast<uint32_t>(ticks-begin);
            if(drawn!=active_ticks/60) { drawn=active_ticks/60; draw(); }
        }
    }
    bool complete=window && !quit && active_ticks>=kDuration;
    bool saved=save(outfd,count,complete);
    if(FSClose(outfd)!=noErr) saved=false;
    if(ready && FlushVol(nullptr,output.vRefNum)!=noErr) saved=false;
    if(infd && FSClose(infd)!=noErr) saved=false;
    if(FSpDelete(&input)!=noErr) saved=false;
    if(window) DisposeWindow(window);
    return saved && complete?0:1;
}
