// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Host-only stand-ins for the small event-manager surface used by SystemEvents.
#pragma once
#include <cstdint>
#define pascal
using OSErr = int16_t;
using AEEventID = uint32_t;
struct AppleEvent {};
using AEEventHandlerUPP = OSErr (*)(const AppleEvent *, AppleEvent *, int32_t);
constexpr OSErr noErr=0, memFullErr=-108, errAEEventNotHandled=-1708;
constexpr uint32_t kCoreEventClass=0x61657674;
constexpr AEEventID kAEOpenApplication=0x6f617070, kAEOpenDocuments=0x6f646f63,
                    kAEPrintDocuments=0x70646f63, kAEQuitApplication=0x71756974;
AEEventHandlerUPP NewAEEventHandlerUPP(AEEventHandlerUPP);
void DisposeAEEventHandlerUPP(AEEventHandlerUPP);
OSErr AEInstallEventHandler(uint32_t,AEEventID,AEEventHandlerUPP,int32_t,bool);
OSErr AERemoveEventHandler(uint32_t,AEEventID,AEEventHandlerUPP,bool);
