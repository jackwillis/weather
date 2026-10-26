// Separate executable: no fixture or marker handling is linked into Weather.
#include "ui/os9/window.hpp"
#include "platform/os9/seed_store_os9.hpp"
#include "platform/os9/config_os9.hpp"
#include "platform/os9/net_os9.hpp"
#include "app/config_load.hpp"
#include <OSUtils.h>
#include <cstdio>
#include <cstring>

using namespace wx;
namespace {
int stage=0, fetches=0, failures=0;
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
unsigned long started=0;
FILE *logfile=nullptr;
Config *test_cfg=nullptr;
void check(bool ok,const char *what) {
	std::fprintf(logfile,"%s stage=%d %s\n",ok?"ok":"FAIL",stage,what);
	if(!ok) ++failures;
	std::fflush(logfile);
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
	test_cfg=&cfg; cfg=Config{}; std::strcpy(cfg.name,"UI test - simulated weather");
	started=TickCount();
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
		if(stage==9) check(m.phase()==Phase::Failed && m.has_forecast() && m.stale(0),"failed update retains stale forecast");
		if(stage==8) check(m.has_forecast() && m.forecast().metric,"unit change refetched metric values");
		if(stage==11) check(m.phase()==Phase::Ok && m.has_forecast(),"live certificate-verified fetch succeeded");
		FILE *ready=std::fopen("weather-ui.ready","w");
		if(!ready) { check(false,"write ready marker"); os9_window_action(WindowAction::Quit); return; }
		std::fprintf(ready,"%d\n",stage); std::fclose(ready); announced=true;
	}
	if(!advance()) return;
	++stage; announced=false;
	if(stage==7) os9_window_action(WindowAction::About);
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
	++fetches; waiting_fetch=false;
	if(stage==12) {
		os9_window_menu_selection((129L<<16)|3); // File > Quit command, while fetching.
		check(!net.yield(net),"quit cancels a yielding fetch");
		wx::Status s=os9_window_fetch_live(cfg,net,f);
		check(s.err==Err::Cancelled,"quit propagates through production fetch without retry/fallback");
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
	int rc=run_os9_window(&hooks);
	check(stage==12 && fetches==12,"all stages completed with exactly twelve fetches");
	std::fprintf(logfile,"finished: %d failures; rc=%d\n",failures,rc);
	std::fclose(logfile); return failures || rc ? 1:0;
}
