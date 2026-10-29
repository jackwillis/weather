// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#pragma once
#include "app/location_search.hpp"
#include <Dialogs.h>
namespace wx {
class PlaceDialog {
    DialogPtr dialog_=nullptr;
    GrafPtr prior_=nullptr;
    WindowPtr parent_=nullptr;
    PlacePicker picker_;
    char query_[64]={};
    bool requested_=false, chosen_=false;
    void sync_query();
    void render();
    void close();
public:
    bool visible() const { return dialog_!=nullptr; }
    WindowPtr window() const { return reinterpret_cast<WindowPtr>(dialog_); }
    bool busy() const { return picker_.busy; }
    bool cancelled() const { return picker_.cancelled; }
    Status open(WindowPtr parent);
    void cancel();
    void draw();
    bool event(EventRecord &);
    void item(int);
    void set_query(sv);
    bool take_request() { bool result=requested_; requested_=false; return result; }
    sv query() const { return query_; }
    Status begin();
    void finish(Status,const PlaceResults &);
    bool take_choice(LocationForm &);
    size_t count() const { return picker_.search.results.count; }
    int selected() const { return picker_.selected; }
};
}
