// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Separate executable: no fixture or marker handling is linked into Weather.
#include "ui/os9/window.hpp"
#include "platform/os9/seed_store_os9.hpp"
#include "platform/os9/config_os9.hpp"
#include "platform/os9/net_os9.hpp"
#include "app/config_load.hpp"
#ifdef WEATHER_CACHE_UI_TEST
#include "platform/os9/cache_os9.hpp"
#include "platform/os9/toolbox_extra.h"
#include "platform/os9/portable.hpp"
#endif
#include <OSUtils.h>
#include <AppleEvents.h>
#include <Processes.h>
#include <cstdio>
#include <cstring>

using namespace wx;
namespace {
int stage=0, fetches=0, failures=0;
bool quit_idle=false;
char isolated_preferences[4096]={};
SettingsStore isolated_store;
void init_store() {
    isolated_store.load=[](SettingsStore &,char *out,size_t cap,size_t *n) {
        *n=std::strlen(isolated_preferences);
        if(*n>=cap) return make(Err::Config,"diagnostic settings too large");
        std::memcpy(out,isolated_preferences,*n); return okay();
    };
    isolated_store.save=[](SettingsStore &,const char *text,size_t n) {
        if(n>=sizeof isolated_preferences) return make(Err::Config,"diagnostic settings too large");
        std::memcpy(isolated_preferences,text,n); isolated_preferences[n]=0; return okay();
    };
}
bool announced=false, waiting_fetch=true;
unsigned long started=0, last_fetch_ticks=0;
FILE *logfile=nullptr;
Config *test_cfg=nullptr;
#ifdef WEATHER_CACHE_UI_TEST
Os9Cache test_cache;
FSSpec cache_dir;
char cache_path[256];
bool cache_owned=false;
OSErr cache_spec(const char *path,FSSpec &out) {
	unsigned char p[256]; size_t n=std::strlen(path); if(n>255) return paramErr;
	p[0]=static_cast<unsigned char>(n); std::memcpy(p+1,path,n); return FSMakeFSSpec(0,0,p,&out);
}
#endif
void check(bool ok,const char *what) {
	std::fprintf(logfile,"%s stage=%d %s\n",ok?"ok":"FAIL",stage,what);
	if(!ok) ++failures;
	std::fflush(logfile);
}
bool send_quit() {
	ProcessSerialNumber psn;
	if(GetCurrentProcess(&psn)!=noErr) return false;
	AEAddressDesc address;
	OSErr err=AECreateDesc(typeProcessSerialNumber,&psn,sizeof psn,&address);
	if(err!=noErr) return false;
	AppleEvent event,reply;
	err=AECreateAppleEvent(kCoreEventClass,kAEQuitApplication,&address,
	                       kAutoGenerateReturnID,kAnyTransactionID,&event);
	AEDisposeDesc(&address);
	if(err!=noErr) return false;
	err=AESend(&event,&reply,static_cast<AESendMode>(kAENoReply)|static_cast<AESendMode>(kAENeverInteract),kAENormalPriority,kAEDefaultTimeout,nullptr,nullptr);
	AEDisposeDesc(&event);
	return err==noErr;
}
bool advance() {
	FILE *f=std::fopen("weather-ui.advance","r");
	if(!f) return false;
	char text[32]; size_t n=std::fread(text,1,sizeof text,f);
	bool read_ok=!std::ferror(f); bool close_ok=std::fclose(f)==0;
	if(!read_ok || !close_ok || n==sizeof text) return false;
	Num ack=parse_fixed(trim(sv(text,n)),0);
	// An acknowledgement is consumed once by its stage number. Do not delete
	// a shared HFS marker while Tiger may still have it open.
	return ack.ok && ack.v==stage;
}
void setup(Config &cfg) {
	FILE *mode=std::fopen("weather-ui.quit-mode","r");
	if(mode) { char value[16]={}; std::fgets(value,sizeof value,mode); std::fclose(mode); quit_idle=std::strcmp(value,"idle\n")==0; }
	test_cfg=&cfg; cfg=Config{}; std::strcpy(cfg.name,"UI test - simulated weather");
	started=TickCount();
#ifdef WEATHER_CACHE_UI_TEST
	char folder[180]; long directory=0;
	std::snprintf(folder,sizeof folder,"MacOS9Lives:weather-dev:cache-ui-%lu",static_cast<unsigned long>(TickCount()));
	cache_owned=cache_spec(folder,cache_dir)==fnfErr && FSpDirCreate(&cache_dir,0,&directory)==noErr;
	check(cache_owned,"create private cache UI directory");
	if(cache_owned) {
		std::snprintf(cache_path,sizeof cache_path,"%s:forecast",folder);
		wx::Status initialized=os9_cache_init(&test_cache,cache_path);
		check(initialized.ok(),"initialize isolated cache UI store");
		if(initialized.ok()) {
			Forecast f; f.source="open-meteo"; f.temp_t10=650; f.code=0; f.is_day=1;
			std::strcpy(f.time,"2026-10-25T12:00");
			UInt32 mac; GetDateTime(&mac); MachineLocation loc; ReadLocation(&loc);
			wx::Status saved=write_cached_forecast(test_cache.base,cache_key(cfg),f,os9::mac_to_unix(mac,loc.u.gmtDelta));
			check(saved.ok(),"write explicit synthetic cache fixture"); cfg.cache=saved.ok();
		}
	}
#endif
	SettingsStore store=os9_config_store("weather-ui-test.conf");
	FILE *f=std::fopen("weather-ui-test.conf","w");
	if(f) { std::fputs("units=metric\nlatitude=1\n",f); std::fclose(f); }
	Config probe;
	check(f && load_config(store,probe).ok() && probe.metric && probe.lat_e4==10000,"native config read");
	f=std::fopen("weather-ui-test.conf","w");
	if(f) { std::fputs("units=imperial\nlatitude=oops\n",f); std::fclose(f); }
	check(f && !load_config(store,probe).ok() && probe.metric && probe.lat_e4==10000,"invalid config leaves previous settings intact");
	f=std::fopen("weather-ui-test.conf","w");
	if(f) { for(int i=0;i<4096;i++) std::fputc(' ',f); std::fclose(f); }
	check(f && !load_config(store,probe).ok(),"overlong native config rejected");
	check(os9_delete_file("weather-ui-test.conf"),"remove diagnostic config");
	check(load_config(store,probe).ok(),"missing native config uses existing defaults");
	uint32_t active_ticks=os9_active_ticks(), raw_ticks=TickCount();
	long long active_ms=os9_active_milliseconds();
	long done; Delay((cfg.timeout_s+1)*60,&done);
	os9_credit_modal_ticks(raw_ticks,TickCount());
	check(static_cast<uint32_t>(os9_active_ticks()-active_ticks)<30,"DNS clock excludes simulated modal pause longer than timeout");
	check(os9_active_milliseconds()-active_ms<500,"socket clock excludes same simulated modal pause");
}
void idle() {
	if(static_cast<unsigned long>(TickCount()-started)>36000) {
		check(false,"ten-minute test watchdog expired"); os9_window_action(WindowAction::Quit); return;
	}
	if(waiting_fetch) return;
	if(!announced) {
		const Model &m=os9_window_model();
		if(stage==0) check(os9_window_buffered(),"offscreen drawing buffer allocated");
#ifdef WEATHER_CACHE_UI_TEST
		if(stage==0) check(m.cached() && m.has_forecast() && m.forecast().temp_t10==650,"initial failure displays restored stale cache");
		if(stage==1) check(!m.cached() && m.phase()==Phase::Ok,"successful update clears cached label");
#endif
		if(stage==9) check(m.phase()==Phase::Failed && m.has_forecast() && m.stale(0),"failed update retains stale forecast");
		if(stage==8) check(m.has_forecast() && m.forecast().metric,"unit change refetched metric values");
		if(stage==12) check(m.has_forecast() && m.phase()==Phase::Ok,"window remains open with previous forecast after cancel");
		if(stage==11) check(m.phase()==Phase::Ok && m.has_forecast(),"live certificate-verified fetch succeeded");
		FILE *ready=std::fopen("weather-ui.ready","w");
		if(!ready) { check(false,"write ready marker"); os9_window_action(WindowAction::Quit); return; }
		std::fprintf(ready,"%d\n",stage); std::fclose(ready); announced=true;
	}
	// Marker acknowledgements can arrive faster than the production manual refresh floor.
	if(static_cast<unsigned long>(TickCount()-last_fetch_ticks)<300) return;
	if(!advance()) return;
	++stage; announced=false;
	if(stage==13 && quit_idle) {
		check(os9_window_model().phase()!=Phase::Fetching,"system quit requested while idle");
		check(send_quit(),"send real core quit Apple Event while idle");
		waiting_fetch=true; // Do not request another fetch before the queued event is dispatched.
	}
	else if(stage==7) os9_window_action(WindowAction::About);
	else if(stage==8) {
		os9_window_action(WindowAction::DismissAbout);
		os9_window_action(WindowAction::ToggleAuto);
		os9_window_action(WindowAction::Metric); waiting_fetch=true;
	} else {
		if(stage==11) std::strcpy(test_cfg->name,"Madison, WI (live test)");
		os9_window_action(WindowAction::Refresh); waiting_fetch=true;
	}
}
wx::Status fetch(const Config &cfg,Net &net,Forecast *f) {
	last_fetch_ticks=TickCount();
	++fetches; waiting_fetch=false;
#ifdef WEATHER_CACHE_UI_TEST
	if(stage==0) return make(Err::Timeout,"simulated initial outage");
#endif
	if(stage==13) {
		check(send_quit(),"send real core quit Apple Event while fetching");
		check(!net.yield(net),"system quit cancels a yielding fetch");
		wx::Status s=os9_window_fetch_live(cfg,net,f);
		check(s.err==Err::Cancelled,"system quit propagates through production fetch without retry/fallback");
		return s;
	}
	if(stage==12) {
		Forecast previous=os9_window_model().forecast();
		os9_window_key('.',true); // Same dispatch as a native keyDown with cmdKey.
		check(!net.yield(net),"Command-period cancels a yielding fetch without quitting");
		wx::Status s=os9_window_fetch_live(cfg,net,f);
		check(s.err==Err::Cancelled,"shortcut propagates through production fetch without retry/fallback");
		check(os9_window_model().has_forecast() && os9_window_model().forecast().temp_t10==previous.temp_t10,
		      "cancel preserves previous forecast during unwind");
		return s;
	}
	if(stage==11) {
		unsigned long began=TickCount();
		for(;;) {
			if(!net.yield(net)) return make(Err::Cancelled,"test cancelled");
			FILE *go=std::fopen("weather-ui.live-go","r");
			if(go) { std::fclose(go); break; }
			if(static_cast<unsigned long>(TickCount()-began)>4200) return make(Err::Timeout,"live-start watchdog");
			long done; Delay(1,&done);
		}
		check(!os9_window_foreground(),"production live fetch starts in background");
		wx::Status s=os9_window_fetch_live(cfg,net,f);
		check(!os9_window_foreground(),"production live fetch completes in background");
		return s;
	}
	if(stage==9) return make(Err::Timeout,"simulated service timeout");
	*f=Forecast{}; f->metric=cfg.metric;
	f->temp_t10=cfg.metric?185:653; f->wind_t10=cfg.metric?129:80; f->humidity=67;
	f->wind_dir=225; f->source="UI fixture (simulated)";
#ifdef WEATHER_CACHE_UI_TEST
	f->source="open-meteo"; // Place label explicitly says simulated; cache schema uses known IDs.
#endif
	std::strcpy(f->time,"2026-10-25T12:00");
	static const int codes[]={0,0,2,65,75,95,45};
	f->code=stage<7?codes[stage]:2; f->is_day=stage!=1;
	if(stage==10) {
		unsigned long updates=os9_window_updates();
		unsigned long suspends=os9_window_suspends(), resumes=os9_window_resumes();
		os9_window_invalidate();
		FILE *ready=std::fopen("weather-ui.ready","w");
		if(ready) { std::fprintf(ready,"10\n"); std::fclose(ready); announced=true; }
		bool non_reentrant=true;
		unsigned long began=TickCount();
		unsigned long pumps=0;
		while(!advance() && static_cast<unsigned long>(TickCount()-began)<5400) {
			os9_window_action(WindowAction::Refresh);
			non_reentrant=non_reentrant && os9_window_model().phase()==Phase::Fetching && fetches==10;
			if(!net.yield(net)) return make(Err::Cancelled,"test cancelled");
			if(++pumps%20==0) {
				FILE *progress=std::fopen("weather-ui.pumps","w");
				if(progress) { std::fprintf(progress,"%lu\n",pumps); std::fclose(progress); }
			}
			long done; Delay(1,&done);
		}
		check(static_cast<unsigned long>(TickCount()-began)<5400,"background stage acknowledged before watchdog");
		check(non_reentrant,"refresh did not reenter fetch");
		check(os9_window_updates()>updates,"update event repainted during yielding fetch");
		check(os9_window_suspends()>suspends && os9_window_resumes()>resumes,"native suspend and resume events handled during fetch");
	}
	return okay();
}
}
int main() {
	logfile=std::fopen("weather-ui-test.log","w"); if(!logfile) return 2;
	init_store();
	WindowTestHooks hooks{setup,idle,fetch,&isolated_store,&isolated_store};
#ifdef WEATHER_CACHE_UI_TEST
	hooks.cache_store=&test_cache.base;
#endif
	int rc=run_os9_window(&hooks);
	Os9SeedStore released_lock;
	bool acquired=released_lock.acquire();
	check(acquired,"managed seed lock released after system quit");
	if(acquired) check(released_lock.release(),"release diagnostic lock check");
	check(stage==13 && fetches==(quit_idle?12:13),"all stages completed with expected fetch count");
#ifdef WEATHER_CACHE_UI_TEST
	if(cache_owned) {
		FSSpec primary;
		check(cache_spec(cache_path,primary)==noErr && FSpDelete(&primary)==noErr,"remove private cache UI record");
		check(FSpDelete(&cache_dir)==noErr,"remove empty cache UI directory");
	}
#endif
	std::fprintf(logfile,"finished: %d failures; rc=%d\n",failures,rc);
	std::fclose(logfile); return failures || rc ? 1:0;
}
