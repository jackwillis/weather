#pragma once
#include "app/preferences.hpp"
#include <Dialogs.h>
namespace wx {
class LocationDialog {
    DialogPtr dialog_ = nullptr;
    GrafPtr prior_ = nullptr;
    LocationEditor editor_;
    int focus_ = 0;
    bool search_requested_=false;
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
    bool take_search_request() { bool value=search_requested_; search_requested_=false; return value; }
    void notice(const char *text) { if(dialog_) message(text); }
    int focus() const { return focus_; }
};
}
