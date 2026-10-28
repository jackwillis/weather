// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#include "ui/os9/system_events.hpp"
#include <cstdio>
#include <cstdlib>
static int installs, removals, disposed, quits, fail_at;
static bool allocation_fails;
static AEEventHandlerUPP handler;
static AEEventID ids[4];
static int32_t refs[4];
static void require(bool ok) { if(!ok) { std::puts("FAIL OS 9 core-event lifecycle"); std::exit(1); } }
static void quit() { ++quits; }
AEEventHandlerUPP NewAEEventHandlerUPP(AEEventHandlerUPP p) { return allocation_fails?nullptr:p; }
void DisposeAEEventHandlerUPP(AEEventHandlerUPP p) { require(p!=nullptr); ++disposed; }
OSErr AEInstallEventHandler(uint32_t cls,AEEventID id,AEEventHandlerUPP p,int32_t ref,bool system) {
    require(cls==kCoreEventClass && !system && installs<4); handler=p;
    ids[installs]=id; refs[installs]=ref; ++installs;
    return installs==fail_at?-50:noErr;
}
OSErr AERemoveEventHandler(uint32_t cls,AEEventID id,AEEventHandlerUPP p,bool system) {
    require(cls==kCoreEventClass && !system && p==handler);
    require(id==ids[installs-(fail_at?1:0)-removals-1]); ++removals; return noErr;
}
static void reset() { installs=removals=disposed=quits=fail_at=0; allocation_fails=false; handler=nullptr; }
int main() {
    reset();
    { wx::SystemEvents events; require(events.start(quit)==noErr);
      require(installs==4 && removals==0);
      require(ids[0]==kAEOpenApplication && ids[1]==kAEOpenDocuments && ids[2]==kAEPrintDocuments && ids[3]==kAEQuitApplication);
      require(handler(nullptr,nullptr,refs[0])==noErr && quits==0);
      require(handler(nullptr,nullptr,refs[1])==noErr && quits==0);
      require(handler(nullptr,nullptr,refs[2])==errAEEventNotHandled && quits==0);
      require(handler(nullptr,nullptr,refs[3])==noErr && quits==1);
      events.stop(); require(removals==4 && disposed==1); events.stop(); require(disposed==1); }
    require(disposed==1);
    for(int fail=1;fail<=4;fail++) {
      reset(); fail_at=fail;
      { wx::SystemEvents events; require(events.start(quit)==-50);
        require(removals==fail-1 && disposed==1); }
      require(disposed==1);
    }
    reset(); allocation_fails=true;
    { wx::SystemEvents events; require(events.start(quit)==memFullErr && installs==0); }
    require(disposed==0);
    reset();
    { wx::SystemEvents events; require(events.start(quit)==noErr); }
    require(removals==4 && disposed==1);
    std::puts("OS 9 core events: registration, dispatch, rollback and teardown passed");
}
