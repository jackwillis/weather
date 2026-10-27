// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// A separately linked, seed-free diagnostic. Never reads production Preferences.
#include "ui/os9/window.hpp"
#include "app/preferences.hpp"
#include "platform/os9/settings_os9.hpp"
#include <Events.h>
#include <cstdio>
#include <cstring>
using namespace wx;
namespace {
FILE *logfile;
int failures=0,fetches=0,stage=0;
bool prepared=false,fail_save=false;
unsigned long started;
char folder[180],path[220];
FSSpec directory;
Os9Settings actual;
SettingsStore prefs,legacy;
void check(bool ok,const char *label) {
    std::fprintf(logfile,"%s stage=%d %s\n",ok?"ok":"FAIL",stage,label);
    if(!ok) ++failures;
    std::fflush(logfile);
}
OSErr spec(const char *name,FSSpec &out) {
    unsigned char p[256]; size_t n=std::strlen(name); if(n>255) return paramErr;
    p[0]=static_cast<unsigned char>(n); std::memcpy(p+1,name,n); return FSMakeFSSpec(0,0,p,&out);
}
bool advance() {
    FILE *f=std::fopen("weather-prefs-ui.advance","r"); if(!f) return false;
    char text[32]; size_t n=std::fread(text,1,sizeof text,f);
    bool ok=!std::ferror(f) && n<sizeof text; if(std::fclose(f)) ok=false;
    Num v=parse_fixed(trim(sv(text,n)),0); return ok && v.ok && v.v==stage;
}
void ready() {
    FILE *f=std::fopen("weather-prefs-ui.ready","w");
    check(f!=nullptr,"create stage marker");
    if(f) { std::fprintf(f,"%d\n",stage); check(std::fclose(f)==0,"close stage marker"); }
}
void setup(Config &cfg) { cfg=Config{}; std::strcpy(cfg.name,"Preferences UI - simulated weather"); }
void fill_valid() {
    os9_window_location_field(0,"10"); os9_window_location_field(1,"20"); os9_window_location_field(2,"Saved place");
}
void idle() {
    if(static_cast<unsigned long>(TickCount()-started)>18000) { check(false,"five-minute watchdog"); os9_window_action(WindowAction::Quit); return; }
    if(!fetches || os9_window_model().phase()==Phase::Fetching) return;
    if(!prepared) {
        if(stage==0) {
            os9_window_menu_selection((130L<<16)|5);
            check(os9_window_location_visible(),"Options Location opens native dialog");
            os9_window_location_field(0,"91"); os9_window_location_key(13);
            check(os9_window_location_visible() && os9_window_location_focus()==0,"invalid latitude stays open and focused");
            os9_window_location_field(0,"10"); os9_window_location_field(1,"181"); os9_window_location_key(13);
            check(os9_window_location_visible() && os9_window_location_focus()==1,"invalid longitude stays open and focused");
            os9_window_location_field(1,"20"); os9_window_location_field(2,"bad#name"); os9_window_location_key(13);
            check(os9_window_location_visible() && os9_window_location_focus()==2,"invalid name stays open and focused");
            check(os9_window_config().lat_e4==430747 && fetches==1 && os9_window_model().has_forecast(),"invalid fields preserve config, forecast and fetch count");
        } else if(stage==1) {
            os9_window_action(WindowAction::Location); fill_valid();
            os9_window_location_item(6); os9_window_location_item(7);
            fail_save=true; os9_window_location_key(13);
            check(os9_window_location_visible() && !os9_window_settings_status().ok(),"save failure remains visible");
            check(os9_window_config().lat_e4==430747 && !os9_window_config().metric && os9_window_model().has_forecast(),"save failure preserves config and old forecast");
        } else if(stage==2) {
            check(fetches==2 && os9_window_config().lat_e4==100000 && os9_window_config().metric && !os9_window_config().auto_refresh,"Apply requests exactly one new-location fetch");
            Os9Settings reopened; Config loaded;
            check(os9_settings_init(&reopened,path).ok() && load_preferences(reopened.base,legacy,loaded).ok() && loaded.lat_e4==100000 && loaded.metric && !loaded.auto_refresh,"fresh adapter reloads persisted coordinates, units and auto setting");
            os9_window_action(WindowAction::Location);
            os9_window_location_key(9); check(os9_window_location_focus()==1,"Tab selects next field");
            os9_window_location_key(9,true); check(os9_window_location_focus()==0,"Shift-Tab selects previous field");
        } else if(stage==3) {
            os9_window_action(WindowAction::Location); os9_window_location_field(0,"80"); os9_window_location_key(27);
            check(!os9_window_location_visible() && os9_window_config().lat_e4==100000 && fetches==2,"Escape cancels without changing location or fetching");
            os9_window_action(WindowAction::Location);
            check(os9_window_location_visible(),"name-only editor reopened after Escape");
            os9_window_location_field(2,"Renamed only");
            check(os9_window_location_visible(),"name-only field updated");
            os9_window_location_key(13);
            check(!os9_window_location_visible() && fetches==2 && os9_window_model().has_forecast(),"name-only Apply preserves forecast and fetch count");
        }
        prepared=true; ready();
    }
    if(!advance()) return;
    if(stage==0) {
        os9_window_location_key(27); check(!os9_window_location_visible() && fetches==1,"Cancel closes invalid editor");
    } else if(stage==1) {
        fail_save=false; os9_window_location_key(13);
        check(!os9_window_location_visible() && !os9_window_model().has_forecast(),"successful Apply closes and clears previous location data");
    } else if(stage==2) os9_window_location_item(2);
    ++stage; prepared=false;
    if(stage==4) os9_window_action(WindowAction::Refresh);
}
wx::Status fetch(const Config &cfg,Net &net,Forecast *out) {
    ++fetches;
    if(stage==4) {
        os9_window_action(WindowAction::Location);
        check(!os9_window_location_visible(),"cannot open editor during a fetch");
        os9_window_action(WindowAction::Quit);
        check(net.yield && !net.yield(net),"Quit cancels through event pump during fetch");
        return make(Err::Cancelled,"diagnostic quit");
    }
    *out=Forecast{}; out->source="Preferences fixture"; out->code=2; out->temp_t10=cfg.metric?150:590;
    out->humidity=50; out->metric=cfg.metric; std::strcpy(out->time,"2026-10-25T00:00");
    return okay();
}
}
int main() {
    logfile=std::fopen("weather-prefs-ui.log","w"); if(!logfile) return 2;
    started=TickCount(); std::snprintf(folder,sizeof folder,"MacOS9Lives:weather-dev:prefs-ui-%lu",started);
    long id=0; bool owned=spec(folder,directory)==fnfErr && FSpDirCreate(&directory,0,&id)==noErr;
    check(owned,"create owned UI test directory"); if(!owned) { std::fclose(logfile); return 1; }
    std::snprintf(path,sizeof path,"%s:settings",folder);
    check(os9_settings_init(&actual,path).ok(),"initialize private Preferences adapter");
    prefs=actual.base;
    prefs.load=[](SettingsStore &,char *text,size_t cap,size_t *n) {
        check(true,"begin native Preferences load");
        wx::Status loaded=actual.base.load(actual.base,text,cap,n);
        check(loaded.ok(),"native Preferences load completed");
        return loaded;
    };
    prefs.save=[](SettingsStore &,const char *text,size_t n) {
        if(fail_save) return make(Err::Io,"Simulated save failure: settings were kept unchanged.");
        check(true,"begin native Preferences save");
        wx::Status saved=actual.base.save(actual.base,text,n);
        check(saved.ok(),"native Preferences save completed");
        return saved;
    };
    legacy.load=[](SettingsStore &,char *,size_t,size_t *n) { *n=0; return okay(); };
    WindowTestHooks hooks{setup,idle,fetch,&prefs,&legacy};
    int result=run_os9_window(&hooks);
    check(stage==4 && fetches==3,"all stages and exactly three scripted fetches");
    FSSpec file;
    check(spec(path,file)==noErr && FSpDelete(&file)==noErr,"remove owned Preferences");
    check(FSpDelete(&directory)==noErr,"remove owned directory");
    std::fprintf(logfile,"finished: %d failures; rc=%d\n",failures,result); std::fclose(logfile);
    return failures || result ? 1:0;
}
