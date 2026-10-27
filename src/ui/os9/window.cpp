// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Native Toolbox window. Network initialization and terminal shutdown occur once.
#include "window.hpp"
#include "app/config_load.hpp"
#include "location.hpp"
#include "place_search.hpp"
#include "platform/os9/settings_os9.hpp"
#include "platform/os9/config_os9.hpp"
#include "core/log.hpp"
#include "core/version.hpp"
#include "app/credits.hpp"
#include "app/logo.hpp"
#include "app/weather_art.hpp"
#include "core/fetch.hpp"
#include "core/providers.hpp"
#include "platform/os9/net_os9.hpp"
#include "tls/tls.hpp"
#include <Quickdraw.h>
#include <Windows.h>
#include <Menus.h>
#include <Fonts.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Events.h>
#include <cstring>

using namespace wx;
namespace {
Config cfg;
Model model;
WindowActions actions;
Net net;
WindowPtr window;
GWorldPtr buffer = nullptr;
const WindowTestHooks *test_hooks = nullptr;
wx::Status startup, settings_notice;
Os9Settings preferences;
SettingsStore legacy;
SettingsStore *prefs_store=nullptr, *legacy_store=nullptr;
LocationDialog location;
PlaceDialog place;
unsigned long update_count = 0;
unsigned long suspend_count = 0, resume_count = 0;
bool foreground = true;
long long painted_at = -1;
bool menus_fetching = false;
bool first_run = false;
constexpr int kHeight = 310;
const unsigned char *pascal_text(const char *s)
{
	// Toolbox calls below consume/copy this result before the next call.
	static unsigned char p[256];
	size_t n = strlen(s);
	if (n > 255) n = 255;
	p[0] = static_cast<unsigned char>(n);
	memcpy(p + 1, s, n);
	return p;
}
Rect box(int x, int y, int w, int h) { return Rect{short(y), short(x), short(y + h), short(x + w)}; }
void color(Rgb c)
{
	RGBColor rgb{static_cast<unsigned short>(c.r * 257), static_cast<unsigned short>(c.g * 257), static_cast<unsigned short>(c.b * 257)};
	RGBForeColor(&rgb);
}
struct Canvas {
	void rect(int x, int y, int w, int h, Rgb c) { color(c); Rect r = box(x, y, w, h); PaintRect(&r); }
	void disc(int x, int y, int r, Rgb c) { color(c); Rect b = box(x - r, y - r, 2 * r, 2 * r); PaintOval(&b); }
	void line(int x, int y, int xx, int yy, int width, Rgb c)
	{
		color(c);
		PenSize(width, width);
		MoveTo(x, y); LineTo(xx, yy);
		PenNormal();
		disc(x, y, width / 2, c); disc(xx, yy, width / 2, c);
	}
	void poly(const ArtPt *p, int n, Rgb c)
	{
		if (!n) return;
		PolyHandle h = OpenPoly();
		if (!h) return;
		MoveTo(p[0].x, p[0].y);
		for (int i = 1; i < n; i++) LineTo(p[i].x, p[i].y);
		LineTo(p[0].x, p[0].y);
		ClosePoly(); color(c); PaintPoly(h); KillPoly(h);
	}
};
void text(int x, int y, const char *s, int size = 12, int width = 308)
{
	color({0, 0, 0});
	TextFont(0); TextSize(size); TextFace(0);
	int n = static_cast<int>(strlen(s));
	while (n && TextWidth(const_cast<char *>(s), 0, n) > width) --n;
	MoveTo(x, y); DrawText(const_cast<char *>(s), 0, n);
}
void wrapped(int x, int y, const char *s, int lines, int width)
{
	TextFont(0); TextSize(10); TextFace(0);
	while (*s && lines--) {
		int n=static_cast<int>(strlen(s));
		while(n && TextWidth(const_cast<char *>(s),0,n)>width) --n;
		if(!n) return;
		if(s[n] && lines) {
			int word=n; while(word && s[word]!=' ') --word;
			if(word) n=word;
		}
		char line[256]; if(n>252) n=252;
		memcpy(line,s,n); line[n]=0;
		if(s[n] && !lines) {
			while(n && TextWidth(line,0,n)+TextWidth(const_cast<char *>("..."),0,3)>width) --n;
			memcpy(line+n,"...",4);
		}
		text(x,y,line,10,width); s+=n; while(*s==' ') ++s; y+=13;
	}
}
constexpr int kAboutPages = 2 + static_cast<int>(kCreditPageCount);   // About, the Credits pages, Seed recovery
int about_page = 0;
void draw_logo(Canvas &c, int x, int y)
{
	for(int row=0;row<kLogoH;row++) {
		const uint8_t *r=kLogoBits+row*kLogoRowBytes;
		int run=-1;
		for(int col=0;col<=kLogoW;col++) {
			bool ink=col<kLogoW && (r[col>>3] & (0x80>>(col&7)));
			if(ink && run<0) run=col;
			if(!ink && run>=0) { c.rect(x+run,y+row,col-run,1,{0,0,0}); run=-1; }
		}
	}
}
void draw_about(Canvas &c)
{
	c.rect(8,43,324,210,{238,238,238}); Rect a=box(8,43,324,210); color({0,0,0}); FrameRect(&a);
	if(about_page==0) {
		draw_logo(c,8+(324-kLogoW)/2,50);
		Buf<48> v; v.add("Weather ").add(WEATHER_VERSION); text(40,150,v.c_str(),12,260);
		for(size_t i=0;i<sizeof kCreditsShort/sizeof *kCreditsShort;i++) text(40,170+static_cast<int>(i)*16,kCreditsShort[i],10,270);
	} else if(about_page<=static_cast<int>(kCreditPageCount)) {
		const CreditPage &pg=kCreditPages[about_page-1];
		Buf<48> title; title.add("Credits: ").add(pg.title); text(24,70,title.c_str(),12,290);
		for(size_t i=0;i<pg.n;i++) text(24,95+static_cast<int>(i)*16,pg.lines[i],10,290);
	} else {
		text(24,70,"Seed recovery",12,290);
		text(24,95,"Forecast source is shown below the data.",10,290);
		text(24,119,"Seed locked? Quit all Weather apps first.",10,290);
		text(24,135,"Never remove a lock while one is running.",10,290);
		text(24,151,"Then follow OS9-SEEDS.md recovery.",10,290);
	}
	Buf<64> foot; foot.add(about_page+1<kAboutPages?"Return: next   Esc: close   ":"Return or Esc: close   ").add_int(about_page+1).add(" of ").add_int(kAboutPages);
	text(24,243,foot.c_str(),9,290);
}
void draw_contents()
{
	Canvas c; c.rect(0,0,340,kHeight,{221,221,221});
	text(16,25,cfg.name);
	if(model.has_forecast()) {
		const Forecast &f=model.forecast(); art::picture(c,{16,43,108,96},f.code,!f.is_day);
		Buf<80> t; t.add_fixed(f.temp_t10,1).add(f.metric?" C":" F"); text(143,79,t.c_str(),24,180);
		text(143,103,wmo_text(f.code),12,180);
		Buf<80> wind; wind.add("Wind ");
		if(f.wind_t10==kUnset) wind.add("unavailable");
		else wind.add_fixed(f.wind_t10,1).add(f.metric?" km/h":" mph");
		text(143,125,wind.c_str(),10,180);
		Buf<80> humidity; humidity.add("Humidity ");
		if(f.humidity==kUnset) humidity.add("unavailable"); else humidity.add_int(f.humidity).add('%');
		text(143,143,humidity.c_str(),10,180);
		Buf<80> observed; observed.add("Observed ").add(f.time).add(" UTC"); text(16,165,observed.c_str(),10);
		text(16,182,f.source?f.source:"",9);
	}
	Buf<256> status;
	const char *seed_error = os9_entropy_error();
	if(!settings_notice.ok()) status.add("Settings: ").add(settings_notice.msg);
	else if (model.phase()==Phase::Failed && window_seed_failure(model.last_error(),seed_error)) status.add(seed_error).add(". See About for recovery.");
	else model.status_line(status,net.now(net));
	wrapped(16,210,status.c_str(),3,308);
	text(16,287,cfg.auto_refresh?"Automatic updates on":"Automatic updates off",9,205);
	Rect r=box(236,270,88,24); color({0,0,0}); FrameRoundRect(&r,8,8); text(248,287,"Refresh",12,70);
	if(actions.about) draw_about(c);
}
void draw()
{
	CGrafPtr saved; GDHandle device; GetGWorld(&saved,&device);
	bool buffered=buffer && LockPixels(GetGWorldPixMap(buffer));
	if(buffered) SetGWorld(buffer,nullptr); else SetPort(window);
	TextFont(0); TextSize(10); draw_contents();
	if(buffered) {
		SetPort(window); Rect r=box(0,0,340,kHeight);
		color({0,0,0}); RGBColor white={65535,65535,65535}; RGBBackColor(&white);
		CopyBits(reinterpret_cast<BitMap *>(*GetGWorldPixMap(buffer)), &window->portBits,&r,&r,srcCopy,nullptr);
		UnlockPixels(GetGWorldPixMap(buffer));
	}
	// Validate explicit redraws too, avoiding a redundant update-event cycle.
	SetPort(window); Rect valid=box(0,0,340,kHeight); ValidRect(&valid);
	SetGWorld(saved,device); painted_at=net.now(net);
}
void settings_committed() {
    if(startup.ok()) return;
    startup=recover_preferences(*prefs_store,*legacy_store,cfg,model,net.now(net));
    if(!startup.ok()) settings_notice=startup;
}
void location_closed() {
    if(location.visible()) return;
    if(location.committed()) { settings_committed(); if(startup.ok()) first_run=false; }
    else if(first_run) actions.apply(WindowAction::Quit,cfg,model,net.now(net));
    draw();
}
void command(WindowAction a) {
    if(actions.quit) return;
    if(a==WindowAction::About && !actions.about) about_page=0;
    if(a==WindowAction::Quit) { actions.apply(a,cfg,model,net.now(net)); return; }
    if(first_run && a!=WindowAction::Location) return;
    if(location.visible()) return;
    if(a==WindowAction::Location) {
        settings_notice=location.open(cfg,model); draw(); return;
    }
    if(a==WindowAction::Metric || a==WindowAction::Imperial || a==WindowAction::ToggleAuto) {
        if(model.phase()==Phase::Fetching) return;
        Config next=cfg;
        if(a==WindowAction::ToggleAuto) next.auto_refresh=!cfg.auto_refresh;
        else next.metric=a==WindowAction::Metric;
        settings_notice=commit_preferences(*prefs_store,*legacy_store,next,cfg,model,net.now(net));
        if(settings_notice.ok()) settings_committed();
        draw(); return;
    }
    if(actions.apply(a,cfg,model,net.now(net))) draw();
}
void menu(long selection)
{
	int id = selection >> 16, item = selection & 65535;
	if (id == 128 && item == 1) command(WindowAction::About);
	if (id == 129 && item == 1) command(WindowAction::Refresh);
	if (id == 129 && item == 3) command(WindowAction::Quit);
	if (id == 130 && item == 1) command(WindowAction::ToggleAuto);
	if (id == 130 && item == 2) command(WindowAction::Metric);
	if (id == 130 && item == 3) command(WindowAction::Imperial);
	if (id == 130 && item == 5) command(WindowAction::Location);
	HiliteMenu(0);
}
struct ModalTrackingTime {
	uint32_t begin = TickCount();
	~ModalTrackingTime() { os9_credit_modal_ticks(begin,TickCount()); }
};

void place_closed() {
    if(place.visible()) return;
    LocationForm form;
    if(place.take_choice(form)) {
        location.set_field(0,form.lat.view()); location.set_field(1,form.lon.view()); location.set_field(2,form.name.view());
    }
}
void service_place_search() {
    if(location.take_search_request() && location.visible()) {
        wx::Status s=place.open(location.window()); if(!s.ok()) location.notice(s.msg);
    }
    if(!place.take_request() || actions.quit) return;
    wx::Status s=place.begin(); if(!s.ok()) return;
    PlaceResults results;
    if(test_hooks) s=test_hooks->search?test_hooks->search(net,place.query(),&results):make(Err::Usage,"Diagnostic has no place lookup");
    else {
        static char body[kGeocodingBodyLimit]; FetchOpts opts;
        opts.attempts=cfg.attempts<3?cfg.attempts:3; opts.timeout_s=cfg.timeout_s<60?cfg.timeout_s:60;
        s=search_places(net,place.query(),opts,body,sizeof body,&results);
    }
    place.finish(s,results); place_closed();
}
bool pump(Net &)
{
	MenuHandle file = GetMenuHandle(129), options = GetMenuHandle(130);
	bool fetching = model.phase() == Phase::Fetching || location.visible() || place.visible();
	MenuHandle apple=GetMenuHandle(128);
	if(location.visible()) DisableItem(apple,1); else EnableItem(apple,1);
	if (fetching != menus_fetching) {
		if (fetching) { DisableItem(file, 1); DisableItem(options, 0); }
		else { EnableItem(file, 1); EnableItem(options, 0); }
		menus_fetching = fetching;
		DrawMenuBar();
	}
	EventRecord e;
	for (int i = 0; i < 8 && !actions.quit && WaitNextEvent(everyEvent, &e, 0, nullptr); i++) {
		if (e.what == updateEvt) {
			WindowPtr w = reinterpret_cast<WindowPtr>(e.message);
			BeginUpdate(w);
			if (w == window) { ++update_count; draw(); }
			else if(w==location.window()) location.draw();
            else if(w==place.window()) place.draw();
			EndUpdate(w);
		} else if (place.visible() && place.event(e)) {
            place_closed();
        } else if (location.visible() && location.event(e,*prefs_store,*legacy_store,cfg,model,net.now(net),&settings_notice)) {
            if(!location.visible()) location_closed();
        } else if (e.what == activateEvt) draw();
		else if (e.what == osEvt) {
			constexpr unsigned long kSuspendResumeMessage = 1;
			constexpr unsigned long kResumeFlag = 1;
			unsigned long message = static_cast<unsigned long>(e.message);
			if ((message >> 24) == kSuspendResumeMessage) {
				foreground = (message & kResumeFlag) != 0;
				if (foreground) { ++resume_count; draw(); }
				else ++suspend_count;
			}
		}
		else if (e.what == keyDown) {
			char ch = e.message & charCodeMask;
			if (e.modifiers & cmdKey) menu(MenuKey(ch));
			else if (ch == 13 && actions.about && about_page + 1 < kAboutPages) { ++about_page; draw(); }
			else if (ch == 13 || ch == 27) command(WindowAction::DismissAbout);
		} else if (e.what == mouseDown) {
			WindowPtr w = nullptr;
			short part = FindWindow(e.where, &w);
			if (part == inMenuBar) {
				long selection;
				{ ModalTrackingTime modal; selection=MenuSelect(e.where); }
				menu(selection);
			} else if (w == window && part == inGoAway) {
				bool close;
				{ ModalTrackingTime modal; close=TrackGoAway(w,e.where); }
				if(close) command(WindowAction::Quit);
			} else if (w == window && part == inDrag) {
				ModalTrackingTime modal;
				Rect bounds = (**GetGrayRgn()).rgnBBox;
				DragWindow(w, e.where, &bounds);
			} else if (w == window && part == inContent) {
				if (FrontWindow()!=window) { SelectWindow(w); continue; }
				SetPort(w);
				Point p = e.where;
				GlobalToLocal(&p);
				if (p.h >= 236 && p.h < 324 && p.v >= 270 && p.v < 294) command(WindowAction::Refresh);
			}
		}
	}
    if((place.visible() || location.visible()) && !actions.quit) {
        EventRecord idle={}; idle.what=nullEvent; idle.when=TickCount();
        if(place.visible()) place.event(idle);
        else location.event(idle,*prefs_store,*legacy_store,cfg,model,net.now(net),&settings_notice);
    }
	if (!actions.quit && net.now(net) != painted_at) draw();
	return !actions.quit && !(place.busy() && place.cancelled());
}
}
namespace wx {
void os9_window_action(WindowAction a) { command(a); }
bool os9_window_place_visible() { return place.visible(); }
void os9_window_place_query(sv value) { if(!actions.quit) place.set_query(value); }
void os9_window_place_item(int item) { if(!actions.quit) { place.item(item); place_closed(); } }
void os9_window_place_key(char key,bool command) {
    if(actions.quit) return;
    EventRecord e={}; e.what=keyDown; e.message=key; e.modifiers=command?cmdKey:0;
    place.event(e); place_closed();
}
size_t os9_window_place_count() { return place.count(); }
int os9_window_place_selected() { return place.selected(); }
bool os9_window_location_visible() { return location.visible(); }
void os9_window_location_field(int f,sv text) { if(!actions.quit && !place.visible()) location.set_field(f,text); }
void os9_window_location_item(int item) {
    if(actions.quit || place.visible()) return;
    bool was_visible=location.visible();
    location.item(item,*prefs_store,*legacy_store,cfg,model,net.now(net),&settings_notice);
    if(was_visible && !location.visible()) location_closed();
}
int os9_window_location_focus() { return location.focus(); }
void os9_window_location_key(char key,bool shift) {
    if(actions.quit || place.visible()) return;
    EventRecord e={}; e.what=keyDown; e.message=key; e.modifiers=shift?shiftKey:0;
    bool visible=location.visible();
    location.event(e,*prefs_store,*legacy_store,cfg,model,net.now(net),&settings_notice);
    if(visible && !location.visible()) location_closed();
}
const Config &os9_window_config() { return cfg; }
Status os9_window_settings_status() { return settings_notice; }
const Model &os9_window_model() { return model; }
unsigned long os9_window_updates() { return update_count; }
bool os9_window_buffered() { return buffer!=nullptr; }
void os9_window_menu_selection(long selection) { menu(selection); }
unsigned long os9_window_suspends() { return suspend_count; }
unsigned long os9_window_resumes() { return resume_count; }
bool os9_window_foreground() { return foreground; }
void os9_window_invalidate() { SetPort(window); Rect r=box(0,0,340,kHeight); InvalRect(&r); }
Status os9_window_fetch_live(const Config &c, Net &n, Forecast *f) {
	static char response[65536];
	Query q; q.lat_e4=c.lat_e4; q.lon_e4=c.lon_e4; q.metric=c.metric;
	FetchOpts opts; opts.timeout_s=c.timeout_s; opts.attempts=c.attempts; opts.contact=c.contact;
	const Provider *providers[8]; size_t count=0;
	Status s=resolve_providers(c.providers,kProviders,kNumProviders,providers,8,&count);
	if(s.ok()) s=fetch_forecast(n,providers,count,q,opts,response,sizeof response,f);
	return s;
}
int run_os9_window(const WindowTestHooks *hooks) {
	test_hooks=hooks;
	legacy=os9_config_store(); legacy_store=&legacy;
    prefs_store=&preferences.base;
    if(hooks) {
        // Diagnostics must supply their own isolated store, never production Preferences.
        if(!hooks->preferences || !hooks->legacy) return 1;
        prefs_store=hooks->preferences; legacy_store=hooks->legacy; startup=okay();
    } else startup=os9_settings_init(&preferences);
    bool configured=false;
    if(hooks && hooks->setup) { hooks->setup(cfg); first_run=hooks->first_run; }
    else if(startup.ok()) { startup=load_preferences(*prefs_store,*legacy_store,cfg,&configured); first_run=startup.ok() && !configured; }
	model=Model(ModelConfig{cfg.auto_refresh,900,60});
	log_init(cfg.level,false,"weather");
	InitGraf(&qd.thePort); InitFonts(); InitWindows(); InitMenus(); TEInit(); InitDialogs(nullptr); InitCursor();
	Rect bounds=box(80,60,340,kHeight);
	window=NewCWindow(nullptr,&bounds,pascal_text("Weather"),true,documentProc,reinterpret_cast<WindowPtr>(-1L),true,0);
	if(!window) return 1;
	SetPort(window); Rect local=box(0,0,340,kHeight);
	if(NewGWorld(&buffer,0,&local,nullptr,nullptr,0)!=noErr) buffer=nullptr;
	MenuHandle apple = NewMenu(128, pascal_text("\024"));
	MenuHandle file = NewMenu(129, pascal_text("File"));
	MenuHandle options = NewMenu(130, pascal_text("Options"));
	if (!apple || !file || !options) {
		if (apple) DisposeMenu(apple);
		if (file) DisposeMenu(file);
		if (options) DisposeMenu(options);
		if (buffer) DisposeGWorld(buffer);
		DisposeWindow(window);
		return 1;
	}
	AppendMenu(apple, pascal_text("About Weather")); InsertMenu(apple, 0);
	AppendMenu(file, pascal_text("Refresh/R;(-;Quit/Q")); InsertMenu(file, 0);
	AppendMenu(options, pascal_text("Automatic updates;Metric;Imperial;(-;Location/L")); InsertMenu(options, 0);
	DrawMenuBar();
	os9_net_init(&net); net.yield=pump; tls_net_init(&net);
    if(first_run) settings_notice=location.open(cfg,model);
    draw();
	while(!actions.quit) {
		pump(net); if(actions.quit) break;
		if(test_hooks && test_hooks->idle) test_hooks->idle();
		if(actions.quit) break;
        service_place_search(); if(actions.quit) break;
		if(!first_run && !location.visible() && !place.visible() && model.tick(net.now(net))) {
			draw(); Forecast f;
			Status s=startup;
			if(s.ok()) s=test_hooks && test_hooks->fetch ? test_hooks->fetch(cfg,net,&f) : os9_window_fetch_live(cfg,net,&f);
			model.finish(net.now(net),s,&f); draw();
		}
		CheckItem(options,1,cfg.auto_refresh); CheckItem(options,2,cfg.metric); CheckItem(options,3,!cfg.metric);
		long finalTicks; Delay(1,&finalTicks);
	}
    place.cancel();
	location.cancel();
	os9_net_shutdown();
	DeleteMenu(128); DeleteMenu(129); DeleteMenu(130);
	DisposeMenu(apple); DisposeMenu(file); DisposeMenu(options);
	if(buffer) DisposeGWorld(buffer);
	DisposeWindow(window); return 0;
}
} // namespace wx
