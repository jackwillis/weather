// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Optional seed-free native place-search acceptance. In-memory stores and lookup.
#include "ui/os9/window.hpp"
#include <Events.h>
#include <cstdio>
#include <cstring>
using namespace wx;
namespace {
FILE *logfile;
SettingsStore prefs,legacy;
char stored[4096]={};
bool fail_save=false;
int failures=0,fetches=0,lookups=0,stage=0;
unsigned long started,stage_started;
int shown_stage=-1;
void check(bool value,const char *message) {
    std::fprintf(logfile,"%s %s\n",value?"ok":"FAIL",message);
    if(!value) ++failures;
    std::fflush(logfile);
}
void setup(Config &cfg) { cfg.auto_refresh=false; }
wx::Status fetch(const Config &,Net &,Forecast *f) {
    ++fetches; f->source="open-meteo"; f->temp_t10=650; f->code=0; return okay();
}
wx::Status lookup(Net &net,sv query,PlaceResults *out) {
    ++lookups;
    if(query=="Cancel") {
        os9_window_place_key(27);
        check(net.yield && !net.yield(net),"Cancel propagates through network yield");
        return make(Err::Cancelled,"fixture cancellation");
    }
    if(query=="Fail") return make(Err::HttpStatus,"fixture lookup failure",503);
    if(query=="Empty") { *out=PlaceResults{}; return okay(); }
    return geocoding_parse(R"({"results":[{"id":1,"name":"Madison","latitude":43.07,"longitude":-89.38,"admin1":"Wisconsin","country_code":"US"},{"id":2,"name":"Paris","latitude":48.85,"longitude":2.35,"country_code":"FR"}]})",out);
}
void search(sv query) { os9_window_place_query(query); os9_window_place_item(4); }
void idle() {
    if(TickCount()-started>60*30) { check(false,"search diagnostic timeout"); os9_window_action(WindowAction::Quit); return; }
    if(stage!=shown_stage) {
        shown_stage=stage; stage_started=TickCount();
        std::fprintf(logfile,"capture-stage: %d\n",stage); std::fflush(logfile);
    }
    // Leave deterministic results/error states visible for an external screenshot driver.
    if((stage==2 || stage==20 || stage==21 || stage==5 || stage==6) && TickCount()-stage_started<180) return;
    if(stage==0) {
        check(os9_window_location_visible() && fetches==0,"first run stays gated");
        os9_window_location_item(12); os9_window_location_item(1);
        check(stored[0]==0 && fetches==0,"queued Apply cannot bypass a pending Find"); ++stage;
    } else if(stage==1) {
        check(os9_window_place_visible() && lookups==0,"Find opens without a lookup");
        os9_window_place_query("Paris");
        check(lookups==0,"typing does not send a request");
        os9_window_place_item(1); check(os9_window_place_visible(),"Use requires explicit selection");
        os9_window_place_key(13); ++stage;
    } else if(stage==2) {
        check(lookups==1 && os9_window_place_count()==2 && os9_window_place_selected()==-1,"ambiguous results have no default selection");
        os9_window_place_key(31);
        check(os9_window_place_selected()==0,"Down selects the first result explicitly");
        os9_window_place_query("Madison");
        check(os9_window_place_count()==0 && os9_window_place_selected()==-1,"query edit discards old results and selection");
        search("Paris"); stage=20;
    } else if(stage==20) {
        os9_window_place_key(31); os9_window_place_key(31); os9_window_place_key(13);
        check(!os9_window_place_visible() && os9_window_location_visible() && stored[0]==0 && fetches==0,"Use fills fields without saving or fetching");
        fail_save=true; os9_window_location_item(1);
        check(os9_window_location_visible() && stored[0]==0 && fetches==0,"failed save keeps first-run gate");
        stage=21;
    } else if(stage==21) {
        fail_save=false; os9_window_location_item(1); stage=3;
    } else if(stage==3 && fetches==1) {
        check(os9_window_config().lat_e4==488500 && !std::strcmp(os9_window_config().name,"Paris"),"Apply commits the explicitly selected place");
        os9_window_action(WindowAction::Location); os9_window_location_item(12); ++stage;
    } else if(stage==4) { search("Fail"); ++stage; }
    else if(stage==5) {
        check(os9_window_place_visible() && os9_window_place_count()==0,"failed lookup has no stale results");
        search("Empty"); ++stage;
    } else if(stage==6) {
        check(os9_window_place_count()==0 && os9_window_place_selected()==-1,"empty results cannot be selected");
        search("Cancel"); ++stage;
    } else if(stage==7) {
        check(!os9_window_place_visible() && os9_window_location_visible() && fetches==1,"cancelled lookup returns to Location without fetching");
        check(os9_window_config().lat_e4==488500,"failed and cancelled searches preserve config");
        os9_window_location_item(2); os9_window_action(WindowAction::Quit); ++stage;
    }
}
}
int main() {
    logfile=std::fopen("place-search-test.log","w"); if(!logfile) return 1;
    prefs.load=[](SettingsStore &,char *out,size_t cap,size_t *n) {
        *n=std::strlen(stored); if(*n>=cap) return make(Err::Config,"fixture too large");
        if(*n) std::memcpy(out,stored,*n);
        return okay();
    };
    prefs.save=[](SettingsStore &,const char *text,size_t n) {
        if(fail_save) return make(Err::Io,"fixture save failure");
        if(n>=sizeof stored) return make(Err::Config,"fixture too large");
        if(n) std::memcpy(stored,text,n);
        stored[n]=0; return okay();
    };
    legacy.load=[](SettingsStore &,char *,size_t,size_t *n) { *n=0; return okay(); };
    WindowTestHooks hooks={setup,idle,fetch,&prefs,&legacy,true,lookup};
    started=TickCount(); int result=run_os9_window(&hooks);
    check(stage==8 && lookups==5 && fetches==1 && stored[0],"all isolated search scenarios finished");
    std::fprintf(logfile,"place-search: %d failures\n",failures); std::fclose(logfile);
    return result || failures?1:0;
}
