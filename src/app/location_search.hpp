// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// A result is copied into editable fields only after an explicit choice.
#pragma once
#include "locform.hpp"
#include "core/geocoding.hpp"
namespace wx {
struct LocationSearch {
    PlaceResults results;
    Status error;
    void clear() { results=PlaceResults{}; error=okay(); }
    Status accept(sv body) { clear(); error=geocoding_parse(body,&results); return error; }
    Status choose(size_t index,LocationForm &form) const {
        if(index>=results.count) return make(Err::Usage,"Choose a place result first");
        const Place &p=results.items[index]; Buf<16> lat,lon;
        lat.add_fixed(p.lat_e4,4); lon.add_fixed(p.lon_e4,4);
        form.lat.set(lat.view()); form.lon.set(lon.view()); form.name.set(p.name);
        form.error=""; return okay(); // Existing Apply/save is still required.
    }
};
// Shared picker policy: no implicit first result, no stale result after edits,
// and no selection while a lookup is in progress or after cancellation.
struct PlacePicker {
    LocationSearch search;
    int selected=-1;
    bool busy=false, cancelled=false;
    void edited() { if(!busy) { search.clear(); selected=-1; cancelled=false; } }
    Status begin(sv query) {
        if(busy) return make(Err::Usage,"Search is already running");
        edited(); char path[256]; search.error=geocoding_path(query,path,sizeof path);
        busy=search.error.ok(); return search.error;
    }
    void cancel() { cancelled=true; selected=-1; search.clear(); }
    void finish(Status status,const PlaceResults &results) {
        busy=false; selected=-1; search.clear();
        if(cancelled) search.error=make(Err::Cancelled,"Search cancelled");
        else if(!status.ok()) search.error=status;
        else if(results.count>kPlaceLimit) search.error=make(Err::BadData,"Too many place results");
        else search.results=results;
    }
    Status select(size_t index) {
        if(busy || cancelled || index>=search.results.count) return make(Err::Usage,"Choose a place result first");
        selected=static_cast<int>(index); return okay();
    }
    Status choose(LocationForm &form) const {
        if(busy || cancelled || selected<0) return make(Err::Usage,"Choose a place result first");
        return search.choose(static_cast<size_t>(selected),form);
    }
};
}
