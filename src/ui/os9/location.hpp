// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#pragma once
#include "app/preferences.hpp"
#include <Dialogs.h>
namespace wx {
class LocationDialog {
    DialogPtr dialog_ = nullptr;
    GrafPtr prior_ = nullptr;
    LocationEditor editor_;
    int focus_ = 0;
    void message(const char *);
    bool read();
public:
    bool visible() const { return dialog_ != nullptr; }
    bool committed() const { return editor_.committed; }
    WindowPtr window() const { return reinterpret_cast<WindowPtr>(dialog_); }
    Status open(const Config &,const Model &);
    void cancel();
    void draw();
    bool event(EventRecord &,SettingsStore &,SettingsStore &,Config &,Model &,long long,Status *);
    bool item(int,SettingsStore &,SettingsStore &,Config &,Model &,long long,Status *);
    // Direct UI operations are also used by the separately linked native probe.
    void set_field(int,sv);
    int focus() const { return focus_; }
};
}
