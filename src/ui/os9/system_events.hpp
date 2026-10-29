// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// One window app owns the core-event handlers until its event loop has stopped.
#pragma once
#include <AppleEvents.h>
#include <cstdint>
namespace wx {
class SystemEvents {
    AEEventHandlerUPP handler_ = nullptr;
    unsigned installed_ = 0;
    static void (*quit_)(); // Defined once: Retro68 can overlap weak inline mutable storage.
    inline static constexpr AEEventID ids_[4] = {
        kAEOpenApplication,kAEOpenDocuments,kAEPrintDocuments,kAEQuitApplication
    };
    static pascal OSErr handle(const AppleEvent *, AppleEvent *, int32_t kind) {
        if(static_cast<AEEventID>(kind)==kAEQuitApplication) {
            if(quit_) quit_(); // Request only: unwind the fetch before closing network/seed state.
            return noErr;
        }
        if(static_cast<AEEventID>(kind)==kAEPrintDocuments) return errAEEventNotHandled;
        return noErr;
    }
public:
    SystemEvents() = default;
    SystemEvents(const SystemEvents &) = delete;
    SystemEvents &operator=(const SystemEvents &) = delete;
    ~SystemEvents() { stop(); }
    OSErr start(void (*request_quit)()) {
        handler_=NewAEEventHandlerUPP(handle);
        if(!handler_) return memFullErr;
        quit_=request_quit;
        while(installed_<4) {
            OSErr err=AEInstallEventHandler(kCoreEventClass,ids_[installed_],handler_,
                                           static_cast<int32_t>(ids_[installed_]),false);
            if(err!=noErr) { stop(); return err; }
            ++installed_;
        }
        return noErr;
    }
    void stop() {
        if(!handler_) return;
        bool removed=true;
        while(installed_) {
            --installed_;
            if(AERemoveEventHandler(kCoreEventClass,ids_[installed_],handler_,false)!=noErr) removed=false;
        }
        quit_=nullptr;
        // If removal unexpectedly fails, retain its descriptor until process exit.
        if(removed) DisposeAEEventHandlerUPP(handler_);
        handler_=nullptr;
    }
};
}
