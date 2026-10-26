// Explicit, seed-free first-run UI acceptance; stores exist only in memory.
#include "ui/os9/window.hpp"
#include <Events.h>
#include <cstdio>
#include <cstring>
using namespace wx;
namespace {
FILE *logfile;
SettingsStore prefs,legacy;
char saved[4096]={};
bool fail_save=false;
int failures=0,fetches=0,stage=0;
unsigned long started;
void check(bool value,const char *message) {
    std::fprintf(logfile,"%s %s\n",value?"ok":"FAIL",message);
    if(!value) ++failures;
    std::fflush(logfile);
}
void setup(Config &cfg) { cfg.auto_refresh=false; }
wx::Status fetch(const Config &,Net &,Forecast *f) {
    ++fetches; f->source="open-meteo"; f->temp_t10=650; f->code=0; return okay();
}
void idle() {
    if(TickCount()-started>60*20) { check(false,"first-location diagnostic timed out"); os9_window_action(WindowAction::Quit); return; }
    if(stage==0) {
        check(os9_window_location_visible() && fetches==0,"first run opens Location before any fetch");
#ifdef WEATHER_FIRST_CANCEL
        os9_window_location_item(2); stage=5;
#else
        os9_window_location_field(0,"91"); os9_window_location_item(1);
        check(os9_window_location_visible() && fetches==0 && !os9_window_settings_status().ok(),"invalid first location cannot fetch");
        stage=1;
#endif
    } else if(stage==1) {
        fail_save=true; os9_window_location_field(0,"12"); os9_window_location_item(1);
        check(os9_window_location_visible() && fetches==0 && os9_window_config().lat_e4!=120000,"failed first save preserves setup gate");
        stage=2;
    } else if(stage==2) {
        fail_save=false; os9_window_location_item(1);
        check(!os9_window_location_visible() && os9_window_config().lat_e4==120000,"successful first save commits location");
        stage=3;
    } else if(stage==3 && fetches==1) {
        check(os9_window_model().has_forecast(),"first save permits exactly one update");
        os9_window_action(WindowAction::Location); os9_window_location_item(2);
        os9_window_action(WindowAction::Refresh); stage=4;
    } else if(stage==4 && fetches==2) { stage=5; os9_window_action(WindowAction::Quit); }
}
}
int main() {
#ifdef WEATHER_FIRST_CANCEL
    logfile=std::fopen("first-location-cancel.log","w");
#else
    logfile=std::fopen("first-location-apply.log","w");
#endif
    if(!logfile) return 1;
    prefs.load=[](SettingsStore &,char *out,size_t cap,size_t *n) {
        *n=std::strlen(saved); if(*n>=cap) return make(Err::Config,"fixture too large");
        std::memcpy(out,saved,*n); return okay();
    };
    prefs.save=[](SettingsStore &,const char *text,size_t n) {
        if(fail_save) return make(Err::Io,"fixture first-save failure");
        if(n>=sizeof saved) return make(Err::Config,"fixture too large");
        std::memcpy(saved,text,n); saved[n]=0; return okay();
    };
    legacy.load=[](SettingsStore &,char *,size_t,size_t *n) { *n=0; return okay(); };
    started=TickCount(); WindowTestHooks hooks{setup,idle,fetch,&prefs,&legacy,true};
    int result=run_os9_window(&hooks);
    check(result==0 && stage==5,"diagnostic completed normally");
#ifdef WEATHER_FIRST_CANCEL
    check(fetches==0 && saved[0]==0,"first-run Cancel neither saves nor fetches");
#else
    check(fetches==2 && saved[0]!=0,"later Cancel is ordinary and manual update remains usable");
#endif
    std::fprintf(logfile,"failures=%d\n",failures); std::fclose(logfile);
    return failures?1:0;
}
