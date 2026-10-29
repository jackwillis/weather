// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// xweather: current conditions in a CDE-look X11 window. One window; menus and dialogs are
// drawn as overlays inside it. All refresh behaviour comes from app/Model; the (blocking)
// fetch keeps the window alive through Net::yield, and can be cancelled by closing it.
#include "app/credits.hpp"
#include "app/locform.hpp"
#include "app/location_search.hpp"
#include "run.hpp"
#include "app/logo.hpp"
#include "app/model.hpp"
#include "cde.hpp"
#include "core/config.hpp"
#include "core/fetch.hpp"
#include "core/log.hpp"
#include "core/providers.hpp"
#include "core/settings.hpp"
#include "core/version.hpp"
#include "platform/posix/net_posix.hpp"
#include "platform/posix/settings_posix.hpp"
#include "platform/posix/cache_posix.hpp"
#include "tls/tls.hpp"
#include <X11/keysym.h>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <getopt.h>
#include <poll.h>

using namespace wx;
using cde::Rect;

namespace {

constexpr int W = 440, H = 300, MB_H = 26, ITEM_H = 22;
constexpr unsigned kF5 = 0xffc2;       // XK_F5 (named here to keep the includes small)
constexpr unsigned kLeftTab = 0xfe20;  // XK_ISO_Left_Tab

enum Mode { Normal, MenuOpen, Location, About, PlaceSearch };
enum Cmd { CmdRefresh = 1, CmdLocation, CmdExit, CmdAuto, CmdCache, CmdImperial, CmdMetric, CmdAbout };
enum Press { PNone = 0, PRefresh, PLocation, POk, PCancel, PAboutOk, PFind, PSearch, PUsePlace, PSearchCancel, PCredits, PNext, PBack };

struct MenuItem { const char *label, *accel; int cmd; int kind; };   // kind: 0 plain, 1 check, 2 radio
const MenuItem kFile[] = {{"Refresh", "F5", CmdRefresh, 0}, {"Location...", "Ctrl+L", CmdLocation, 0}, {"Exit", "Ctrl+Q", CmdExit, 0}};
const MenuItem kOptions[] = {{"Auto-refresh", "", CmdAuto, 1}, {"Imperial (F, mph)", "", CmdImperial, 2}, {"Metric (C, km/h)", "", CmdMetric, 2}, {"Save forecasts on this computer", "", CmdCache, 1}};
const MenuItem kHelp[] = {{"About xweather...", "", CmdAbout, 0}};
struct Menu { const char *title; const MenuItem *items; int n; Rect bar; Rect drop; };

struct App {
	Display *dpy = nullptr;
	Window win = 0;
	Pixmap pm = 0;
	cde::Theme th;
	Atom wm_delete = 0;
	Net net;
	Model model;
	Config cfg;
	PosixStore store;
	bool have_store = false;
	PosixCache cache;
	bool have_cache = false;
	FetchOpts fo;
	char forcehost[256] = "";
	const Provider *providers[8];
	size_t nprov = 0;
	Menu menus[3];
	Mode mode = Normal;
	int about_page = -1;   // -1: About; 0..kCreditPageCount-1: that Credits page
	int menu_open = -1, menu_hover = -1;
	bool menu_by_press = false;   // opened by a click (a second click on its title closes it), not by hovering
	Press pressed = PNone;
	LocationForm form;
    LocationSearch search;
    TextEdit<64> search_query;
    PlaceLookup lookup=nullptr;
    int selected_place=-1, search_focus=0;
    bool searching=false, search_cancel=false;
    const char *search_message="Enter a place, then Search.";
	char notice[160] = "";
	long long notice_until = 0;
	bool quit = false, fetching = false, dirty = true;
	long long last_spin_ms = 0;
};
App g;

long long mono_ms()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return static_cast<long long>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}
long long now_s() { return g.net.now(g.net); }

// ---------------------------------------------------------------- layout (shared by drawing and hit-testing)
Rect r_icon() { return Rect{14, 40, 124, 124}; }
Rect r_refresh() { return Rect{18, 218, 92, 26}; }
Rect r_locbtn() { return Rect{130, 218, 104, 26}; }
Rect r_status() { return Rect{8, 254, W - 16, 40}; }
Rect d_panel() { return Rect{50, 30, 340, 238}; }
Rect d_field(int i) { Rect p = d_panel(); return Rect{p.x + 104, p.y + 42 + i * 34, 200, 24}; }
Rect d_units() { Rect p = d_panel(); return Rect{p.x + 16, p.y + 148, 214, 22}; }
Rect d_ok() { Rect p = d_panel(); return Rect{p.x + 60, p.y + 200, 96, 26}; }
Rect d_cancel() { Rect p = d_panel(); return Rect{p.x + 184, p.y + 200, 96, 26}; }
Rect a_panel() { return Rect{50, 20, 340, 260}; }
Rect a_ok() { Rect p = a_panel(); return Rect{p.x + 194, p.y + 218, 96, 26}; }        // About: OK; Credits: Next
Rect a_credits() { Rect p = a_panel(); return Rect{p.x + 50, p.y + 218, 96, 26}; }   // About: Credits...; Credits: Back

Rect d_find() { return Rect{290,178,84,22}; }
Rect s_query() { return Rect{26,44,280,24}; }
Rect s_search() { return Rect{316,44,96,24}; }
Rect s_row(int i) { return Rect{26,76+28*i,386,28}; }
Rect s_use() { return Rect{130,255,80,24}; }
Rect s_cancel() { return Rect{230,255,88,24}; }

bool menu_checked(const MenuItem &m)
{
	switch (m.cmd) {
	case CmdCache: return g.cfg.cache;
	case CmdAuto: return g.cfg.auto_refresh;
	case CmdImperial: return !g.cfg.metric;
	case CmdMetric: return g.cfg.metric;
	}
	return false;
}

void layout_menus()
{
	static const struct { const char *t; const MenuItem *it; int n; } defs[3] = {
		{"File", kFile, 3}, {"Options", kOptions, 4}, {"Help", kHelp, 1}};
	int x = 6;
	for (int i = 0; i < 3; i++) {
		Menu &m = g.menus[i];
		m.title = defs[i].t; m.items = defs[i].it; m.n = defs[i].n;
		int tw = cde::text_width(g.th.bold, m.title);
		m.bar = Rect{x, 2, tw + 20, MB_H - 4};
		int wmax = 0;
		for (int k = 0; k < m.n; k++) {
			int w = cde::text_width(g.th.normal, m.items[k].label) + cde::text_width(g.th.normal, m.items[k].accel) + 70;
			if (w > wmax) wmax = w;
		}
		m.drop = Rect{x, MB_H, wmax, m.n * ITEM_H + 6};
		x += m.bar.w + 2;
	}
}

// ---------------------------------------------------------------- drawing
void draw_menubar(Drawable d)
{
	cde::Theme &t = g.th;
	cde::fill(t, d, Rect{0, 0, W, MB_H}, t.bg);
	cde::shadow(t, d, Rect{0, 0, W, MB_H}, 2, true);
	for (int i = 0; i < 3; i++) {
		const Menu &m = g.menus[i];
		bool open = g.mode == MenuOpen && g.menu_open == i;
		if (open) { cde::fill(t, d, m.bar, t.sel); cde::shadow(t, d, m.bar, 2, false); }
		cde::text_center(t, d, t.bold, m.bar, m.title, t.fg);
	}
}

void draw_dropdown(Drawable d)
{
	if (g.mode != MenuOpen || g.menu_open < 0) return;
	cde::Theme &t = g.th;
	const Menu &m = g.menus[g.menu_open];
	cde::fill(t, d, m.drop, t.bg);
	cde::shadow(t, d, m.drop, 2, true);
	for (int k = 0; k < m.n; k++) {
		const MenuItem &it = m.items[k];
		Rect row{m.drop.x + 3, m.drop.y + 3 + k * ITEM_H, m.drop.w - 6, ITEM_H};
		if (k == g.menu_hover) { cde::fill(t, d, row, t.sel); cde::shadow(t, d, row, 1, true); }
		if (it.kind != 0 && menu_checked(it)) {
			Rect ind{row.x + 6, row.y + 5, 12, 12};
			if (it.kind == 1) { cde::fill(t, d, ind, t.fg); } else { cde::toggle(t, d, Rect{row.x + 2, row.y, 20, ITEM_H}, "", true, true); }
		}
		int base = row.y + (row.h + t.normal->ascent - t.normal->descent) / 2;
		cde::text(t, d, t.normal, row.x + 26, base, it.label, t.fg);
		if (it.accel[0]) {
			int aw = cde::text_width(t.normal, it.accel);
			cde::text(t, d, t.normal, row.x + row.w - aw - 8, base, it.accel, t.fg);
		}
	}
}

void draw_body(Drawable d)
{
	cde::Theme &t = g.th;
	const Forecast &f = g.model.forecast();
	bool have = g.model.has_forecast();
	Rect ic = r_icon();
	cde::fill(t, d, ic, t.bg);
	cde::shadow(t, d, ic, 2, false);
	cde::weather_icon(t, d, ic.inset(3), have ? f.code : -1, have && f.is_day == 0);

	int x0 = 156, y = 62;
	char name[64];
	if (g.cfg.name[0]) snprintf(name, sizeof name, "%s", g.cfg.name);
	else { Buf<40> c; c.add_fixed(g.cfg.lat_e4, 4).add(", ").add_fixed(g.cfg.lon_e4, 4); snprintf(name, sizeof name, "%s", c.c_str()); }
	char fitted[64];
	cde::fit_text(t.bold, name, W - x0 - 12, fitted, sizeof fitted);
	cde::text(t, d, t.bold, x0, y, fitted, t.fg);
	cde::fill(t, d, Rect{x0, y + 6, W - x0 - 14, 1}, t.bot);
	cde::fill(t, d, Rect{x0, y + 7, W - x0 - 14, 1}, t.top);

	if (!have) {
		cde::text(t, d, t.bold, x0, 118, g.model.phase() == Phase::Failed ? "No weather available" : "Loading weather...", t.fg);
		return;
	}
	Buf<24> temp;
	temp.add_fixed(f.temp_t10, 1).add("\260").add(f.metric ? "C" : "F");
	bool old_data = g.model.stale(now_s());   // cached, failed, old or clock-stepped: dim the values (#62)
	cde::text(t, d, t.big, x0, 108, temp.view(), old_data ? t.dim : t.fg);
	cde::text(t, d, t.bold, x0, 132, wmo_text(f.code), old_data ? t.dim : t.fg);
	Buf<64> wind;
	if (f.wind_t10 != kUnset) {
		wind.add("Wind ").add_fixed(f.wind_t10, 1).add(f.metric ? " km/h" : " mph");
		if (f.wind_dir != kUnset) wind.add(" from ").add(compass(f.wind_dir));
		cde::text(t, d, t.normal, x0, 154, wind.view(), t.fg);
	}
	if (f.humidity != kUnset) {
		Buf<32> hum; hum.add("Humidity ").add_int(f.humidity).add("%");
		cde::text(t, d, t.normal, x0, 172, hum.view(), t.fg);
	}
	Buf<48> obs; obs.add("Forecast for ");
	for (const char *p = f.time; *p; p++) obs.add(*p == 'T' ? ' ' : *p);
	obs.add(" UTC");
	cde::text(t, d, t.normal, x0, 192, obs.view(), t.dim);
}

void draw_status(Drawable d)
{
	cde::Theme &t = g.th;
	Rect s = r_status();
	cde::fill(t, d, s, t.bg);
	cde::shadow(t, d, s, 2, false);
	long long now = now_s();
	Buf<220> st;
	if (g.fetching) {
		g.model.cached_status(st,now);
		static const char *dots[] = {"", ".", "..", "..."};
		st.add("Fetching").add(dots[(mono_ms() / 300) % 4]);
		if (g.model.has_forecast() && !g.model.cached()) { st.add(" (showing data from "); g.model.add_data_age(st, now); st.add(")"); }
	} else {
		g.model.status_line(st, now);
	}
	sv line1 = st.view(), line2;
	size_t nx = find(line1, ". Next update");
	if (nx != sv::npos) { line2 = line1.substr(nx + 2); line1 = line1.substr(0, nx); }
	if (g.notice[0] && now < g.notice_until) line2 = g.notice;
	char buf[220];
	int maxw = s.w - 16;
	cde::fit_text(t.normal, line1, maxw, buf, sizeof buf);
	cde::text(t, d, t.normal, s.x + 8, s.y + 16, buf, t.fg);
	cde::fit_text(t.normal, line2, maxw, buf, sizeof buf);
	cde::text(t, d, t.normal, s.x + 8, s.y + 33, buf, t.dim);
}

void draw_panel(Drawable d, Rect p, const char *title)
{
	cde::Theme &t = g.th;
	cde::fill(t, d, p, t.bg);
	cde::shadow(t, d, p, 2, true);
	cde::shadow(t, d, p.inset(2), 1, true);
	cde::text(t, d, t.bold, p.x + 16, p.y + 26, title, t.fg);
}

void draw_location(Drawable d)
{
	cde::Theme &t = g.th;
	Rect p = d_panel();
	draw_panel(d, p, "Location");
	static const char *labels[3] = {"Latitude:", "Longitude:", "Name:"};
	const char *vals[3] = {g.form.lat.s, g.form.lon.s, g.form.name.s};
	size_t lens[3] = {g.form.lat.n, g.form.lon.n, g.form.name.n};
	size_t curs[3] = {g.form.lat.cur, g.form.lon.cur, g.form.name.cur};
	for (int i = 0; i < 3; i++) {
		Rect f = d_field(i);
		cde::text(t, d, t.normal, p.x + 16, f.y + (f.h + t.normal->ascent - t.normal->descent) / 2, labels[i], t.fg);
		cde::field(t, d, f, vals[i], lens[i], curs[i], g.form.focus == i);
	}
	Rect u = d_units();
	cde::toggle(t, d, u, "Metric units (C, km/h)", g.form.metric, false);
	if (g.form.focus == LocationForm::Units) {
		XSetForeground(t.dpy, t.gc, t.fg);
		XDrawRectangle(t.dpy, d, t.gc, u.x - 2, u.y - 1, 240, static_cast<unsigned>(u.h + 1));
	}
	if (g.form.error[0]) {
		char buf[120];
		cde::fit_text(t.normal, g.form.error, p.w - 32, buf, sizeof buf);
		cde::text(t, d, t.normal, p.x + 16, p.y + 186, buf, t.rgb(150, 20, 20));
	}
    cde::button(t,d,d_find(),"Find...",g.pressed==PFind,false);
	cde::button(t, d, d_ok(), "OK", g.pressed == POk, true);
	cde::button(t, d, d_cancel(), "Cancel", g.pressed == PCancel, false);
}

void draw_search(Drawable d)
{
    auto &t=g.th; draw_panel(d,Rect{12,12,416,276},"Find a place (ASCII query)");
    cde::field(t,d,s_query(),g.search_query.s,g.search_query.n,g.search_query.cur,g.search_focus==0);
    cde::button(t,d,s_search(),"Search",g.pressed==PSearch,g.search_focus==1);
    for(size_t i=0;i<g.search.results.count;i++) {
        Rect row=s_row(static_cast<int>(i));
        if(g.selected_place==static_cast<int>(i)) cde::fill(t,d,row,t.sel);
        const Place &p=g.search.results.items[i]; Buf<128> label;
        label.add(p.name); if(p.region[0]) label.add(", ").add(p.region);
        char fitted[128]; cde::fit_text(t.normal,label.view(),row.w-8,fitted,sizeof fitted);
        cde::text(t,d,t.normal,row.x+4,row.y+12,fitted,t.fg);
        Buf<80> coordinates; if(p.country[0]) coordinates.add(p.country).add(" ");
        coordinates.add_fixed(p.lat_e4,4).add(", ").add_fixed(p.lon_e4,4);
        if(p.simplified) coordinates.add(" (ASCII simplified)");
        cde::text(t,d,t.normal,row.x+4,row.y+25,coordinates.view(),t.fg);
    }
    cde::text(t,d,t.normal,26,232,"Open-Meteo / GeoNames",t.fg);
    char message[128]; cde::fit_text(t.normal,g.searching?"Searching... Escape cancels":g.search_message,386,message,sizeof message);
    cde::text(t,d,t.normal,26,247,message,t.fg);
    cde::button(t,d,s_use(),"Use",g.pressed==PUsePlace,g.search_focus==3);
    cde::button(t,d,s_cancel(),"Cancel",g.pressed==PSearchCancel,g.search_focus==4);
}

void draw_about(Drawable d)
{
	cde::Theme &t = g.th;
	Rect p = a_panel();
	Buf<48> v; v.add("xweather ").add(WEATHER_VERSION);
	if (g.about_page < 0) {
		draw_panel(d, p, "About xweather");
		cde::bitmap1(t, d, p.x + (p.w - kLogoW) / 2, p.y + 36, kLogoBits, kLogoW, kLogoH, kLogoRowBytes, t.fg);
		cde::text(t, d, t.bold, p.x + 16, p.y + 144, v.view(), t.fg);
		for (size_t i = 0; i < sizeof kCreditsShort / sizeof *kCreditsShort; i++)
			cde::text(t, d, t.normal, p.x + 16, p.y + 164 + static_cast<int>(i) * 16, kCreditsShort[i], t.fg);
		cde::button(t, d, a_credits(), "Credits...", g.pressed == PCredits, false);
		cde::button(t, d, a_ok(), "OK", g.pressed == PAboutOk, true);
		return;
	}
	const CreditPage &pg = kCreditPages[g.about_page];
	Buf<48> title; title.add("Credits: ").add(pg.title);
	draw_panel(d, p, title.c_str());
	for (size_t i = 0; i < pg.n; i++)
		cde::text(t, d, t.normal, p.x + 16, p.y + 56 + static_cast<int>(i) * 18, pg.lines[i], t.fg);
	Buf<16> n; n.add_int(g.about_page + 1).add(" of ").add_int(static_cast<long long>(kCreditPageCount));
	cde::text(t, d, t.normal, p.x + 16, p.y + 206, n.c_str(), t.dim);
	cde::button(t, d, a_credits(), "Back", g.pressed == PBack, false);
	cde::button(t, d, a_ok(), "Next", g.pressed == PNext, true);
}

void redraw()
{
	cde::Theme &t = g.th;
	Drawable d = g.pm;
	cde::fill(t, d, Rect{0, 0, W, H}, t.bg);
	draw_body(d);
	cde::button(t, d, r_refresh(), "Refresh", g.pressed == PRefresh, true);
	cde::button(t, d, r_locbtn(), "Location...", g.pressed == PLocation, false);
	draw_status(d);
	draw_menubar(d);
	draw_dropdown(d);
	if (g.mode == Location) draw_location(d);
	if (g.mode == About) draw_about(d);
    if(g.mode==PlaceSearch) draw_search(d);
	XCopyArea(g.dpy, g.pm, g.win, t.gc, 0, 0, W, H, 0, 0);
	XFlush(g.dpy);
	g.dirty = false;
}

// ---------------------------------------------------------------- actions
void set_notice(const char *msg, int secs = 8)
{
	snprintf(g.notice, sizeof g.notice, "%s", msg);
	g.notice_until = now_s() + secs;
	g.dirty = true;
}

void save_kv(const KeyVal *kv, size_t n)
{
	if (!g.have_store) { set_notice("Settings not saved: no HOME and no -c file"); return; }
	Status s = settings_save_kv(g.store.base, kv, n);
	if (!s.ok()) {
		char m[160];
		snprintf(m, sizeof m, "Could not save settings: %s", s.msg);
		set_notice(m);
		logf(LogWarn, "saving settings failed: %s: %s (detail %d)", err_name(s.err), s.msg, s.detail);
	}
}

void open_location()
{
	g.form.load(g.cfg);
	g.mode = Location;
	g.pressed = PNone;
	g.dirty = true;
}

void apply_location()
{
	if (g.mode != Location) return;
	g.pressed = PNone;
	Config c = g.cfg;
	if (!g.form.read(&c).ok()) { g.dirty = true; return; }
    if(!g.have_store) { g.form.error="No settings file is available; location was not changed."; g.dirty=true; return; }
    Status saved=settings_save(g.store.base,c);
    if(!saved.ok()) { g.form.error=saved.msg; g.dirty=true; return; }
    bool changed=c.lat_e4!=g.cfg.lat_e4 || c.lon_e4!=g.cfg.lon_e4 || c.metric!=g.cfg.metric;
    g.cfg=c;
    if(changed) g.model.location_changed();
	g.mode = Normal;
	g.dirty = true;
}

void open_search()
{
    g.search.clear(); g.search_query.set(g.form.name.view());
    g.selected_place=-1; g.search_focus=0; g.search_cancel=false;
    g.search_message="Enter a place, then Search."; g.mode=PlaceSearch; g.dirty=true;
}
void use_place()
{
    if(g.selected_place<0) { g.search_message="Select a result first."; return; }
    Status s=g.search.choose(static_cast<size_t>(g.selected_place),g.form);
    if(s.ok()) { g.mode=Location; g.form.focus=LocationForm::Name; }
    else g.search_message=s.msg;
}
void do_search()
{
    g.search.clear(); g.selected_place=-1;
    char path[256]; Status status=geocoding_path(g.search_query.view(),path,sizeof path);
    if(!status.ok()) { g.search_message=status.msg; return; }
    if(g.fo.force_host && !g.lookup) { g.search_message="Place search requires TLS; leave HTTP test mode."; return; }
    g.searching=true; g.search_cancel=false; redraw();
    static char buffer[40*1024]; FetchOpts opts=g.fo;
    if(opts.attempts>3) opts.attempts=3;
    if(opts.timeout_s>60) opts.timeout_s=60;
    status=(g.lookup?g.lookup:search_places)(g.net,g.search_query.view(),opts,buffer,sizeof buffer,&g.search.results);
    g.searching=false; g.dirty=true;
    if(g.quit) return;
    if(g.search_cancel || status.err==Err::Cancelled) { g.mode=Location; g.search.clear(); }
    else if(!status.ok()) { g.search.clear(); g.search_message=status.msg; }
    else g.search_message=g.search.results.count?"Select a result, then Use; OK saves the location.":"No matching places.";
    g.search_cancel=false;
}

void run_command(int cmd)
{
	g.mode = Normal;
	g.menu_open = -1;
	g.dirty = true;
	switch (cmd) {
	case CmdRefresh: g.model.request_refresh(); break;
	case CmdLocation: open_location(); break;
	case CmdExit: g.quit = true; break;
	case CmdAbout: g.mode = About; g.about_page = -1; break;
    case CmdCache: {
        bool enabled=!g.cfg.cache;
        if(!g.have_cache) { set_notice("Forecast storage unavailable"); break; }
        if(!enabled) {
            Status removed=remove_cached_forecast(g.cache.base);
            if(!removed.ok()) { set_notice(removed.msg); break; }
        }
        KeyVal kv[]={{"cache",enabled?"on":"off"}};
        Status saved=settings_save_kv(g.store.base,kv,1);
        if(!saved.ok()) { set_notice(saved.msg); break; }
        g.cfg.cache=enabled;
        set_notice(enabled?"Forecast saving on":"Forecast saving off; saved forecast removed",15);
        break;
    }
	case CmdAuto: {
		g.cfg.auto_refresh = !g.cfg.auto_refresh;
		g.model.set_auto_refresh(g.cfg.auto_refresh, now_s());
		KeyVal kv[] = {{"autorefresh", g.cfg.auto_refresh ? "on" : "off"}};
		save_kv(kv, 1);
		break;
	}
	case CmdImperial: case CmdMetric: {
		bool metric = cmd == CmdMetric;
		if (metric == g.cfg.metric) break;
		g.cfg.metric = metric;
		KeyVal kv[] = {{"units", metric ? "metric" : "imperial"}};
		save_kv(kv, 1);
		g.model.location_changed();      // the numbers in the old units are no longer right
		break;
	}
	}
}

// ---------------------------------------------------------------- input
int menu_at_bar(int x, int y)
{
	if (y >= MB_H) return -1;
	for (int i = 0; i < 3; i++)
		if (g.menus[i].bar.contains(x, y)) return i;
	return -1;
}
int item_at(int x, int y)
{
	if (g.menu_open < 0) return -1;
	const Menu &m = g.menus[g.menu_open];
	if (!m.drop.contains(x, y)) return -1;
	int k = (y - m.drop.y - 3) / ITEM_H;
	return k >= 0 && k < m.n ? k : -1;
}

void field_click(int field, int x)
{
	TextEdit<16> *t16 = field == LocationForm::Lat ? &g.form.lat : field == LocationForm::Lon ? &g.form.lon : nullptr;
	const char *s = t16 ? t16->s : g.form.name.s;
	size_t n = t16 ? t16->n : g.form.name.n;
	int rel = x - (d_field(field).x + 6);
	size_t cursor = t16 ? t16->cur : g.form.name.cur;
	size_t pos = cde::field_position(g.th.normal, sv(s, n), cursor, d_field(field).w - 12, rel);
	if (t16) t16->set_cursor(pos); else g.form.name.set_cursor(pos);
}

void on_button_press(const XButtonEvent &e)
{
	if (e.button != Button1) return;
	g.pressed = PNone;
	g.dirty = true;
	if (g.mode == MenuOpen || (g.mode == Normal && e.y < MB_H)) {
		int bar = menu_at_bar(e.x, e.y);
		if (bar >= 0) {
			if (g.mode == MenuOpen && g.menu_open == bar && g.menu_by_press) { g.mode = Normal; g.menu_open = -1; }
			else { g.mode = MenuOpen; g.menu_open = bar; g.menu_hover = -1; g.menu_by_press = true; }
			return;
		}
		int k = item_at(e.x, e.y);
		if (g.mode == MenuOpen && k >= 0) { run_command(g.menus[g.menu_open].items[k].cmd); return; }
		g.mode = Normal; g.menu_open = -1;
		return;
	}
    if(g.mode==PlaceSearch) {
        if(s_query().contains(e.x,e.y)) { g.search_focus=0; g.search_query.end(); }
        else if(s_search().contains(e.x,e.y)) g.pressed=PSearch;
        else if(s_use().contains(e.x,e.y)) g.pressed=PUsePlace;
        else if(s_cancel().contains(e.x,e.y)) g.pressed=PSearchCancel;
        else for(size_t i=0;i<g.search.results.count;i++) if(s_row(static_cast<int>(i)).contains(e.x,e.y)) { g.selected_place=static_cast<int>(i); g.search_focus=2; }
        return;
    }
	if (g.mode == Location) {
        if(d_find().contains(e.x,e.y)) { g.pressed=PFind; return; }
		for (int i = 0; i < 3; i++)
			if (d_field(i).contains(e.x, e.y)) { g.form.focus = i; field_click(i, e.x); return; }
		if (d_units().contains(e.x, e.y)) { g.form.focus = LocationForm::Units; g.form.metric = !g.form.metric; return; }
		if (d_ok().contains(e.x, e.y)) g.pressed = POk;
		else if (d_cancel().contains(e.x, e.y)) g.pressed = PCancel;
		return;
	}
	if (g.mode == About) {
		bool credits = g.about_page >= 0;
		if (a_ok().contains(e.x, e.y)) g.pressed = credits ? PNext : PAboutOk;
		else if (a_credits().contains(e.x, e.y)) g.pressed = credits ? PBack : PCredits;
		return;
	}
	if (r_refresh().contains(e.x, e.y)) g.pressed = PRefresh;
	else if (r_locbtn().contains(e.x, e.y)) g.pressed = PLocation;
}

void on_button_release(const XButtonEvent &e)
{
	if (e.button != Button1) return;
	Press p = g.pressed;
	g.pressed = PNone;
	g.dirty = true;
	if (p == PNone) return;
	// A press belongs to the mode in which it began. Dismissing a dialog or
	// accepting it with the keyboard must not replay that press on release.
	if ((p == PRefresh || p == PLocation) ? g.mode != Normal :
	    (p == POk || p == PCancel || p == PFind) ? g.mode != Location :
	    (p == PSearch || p == PUsePlace || p == PSearchCancel) ? g.mode != PlaceSearch : g.mode != About) return;
	Rect r = p == PRefresh ? r_refresh() : p == PLocation ? r_locbtn() : p == POk ? d_ok() : p == PCancel ? d_cancel() : p==PFind?d_find():p==PSearch?s_search():p==PUsePlace?s_use():p==PSearchCancel?s_cancel():(p == PCredits || p == PBack) ? a_credits() : a_ok();
	if (!r.contains(e.x, e.y)) return;
	switch (p) {
	case PRefresh: g.model.request_refresh(); break;
	case PLocation: open_location(); break;
	case POk: apply_location(); break;
    case PFind: open_search(); break;
    case PSearch: do_search(); break;
    case PUsePlace: use_place(); break;
    case PSearchCancel: g.mode=Location; g.search.clear(); break;
	case PCancel: g.mode=Normal; break;
	case PAboutOk: g.mode = Normal; g.about_page = -1; break;
	case PCredits: g.about_page = 0; break;
	case PNext: g.about_page = g.about_page + 1 < static_cast<int>(kCreditPageCount) ? g.about_page + 1 : -1; break;
	case PBack: g.about_page = -1; break;
	default: break;
	}
}

void on_motion(const XMotionEvent &e)
{
	if (g.mode != MenuOpen) return;
	int bar = menu_at_bar(e.x, e.y);
	if (bar >= 0 && bar != g.menu_open) { g.menu_open = bar; g.menu_hover = -1; g.menu_by_press = false; g.dirty = true; return; }
	int k = item_at(e.x, e.y);
	if (k != g.menu_hover) { g.menu_hover = k; g.dirty = true; }
}

void on_key(XKeyEvent &e)
{
	char buf[16];
	KeySym ks = NoSymbol;
	int n = XLookupString(&e, buf, sizeof buf, &ks, nullptr);
	bool ctrl = (e.state & ControlMask) != 0;
	g.dirty = true;
	if (ctrl && (ks == XK_q || ks == XK_Q)) { g.quit = true; return; }
    if(g.mode==PlaceSearch) {
        if(ks==XK_Escape) { g.mode=Location; g.search.clear(); return; }
        if(ks==XK_Tab || ks==kLeftTab) { g.search_focus=(g.search_focus+(ks==kLeftTab?4:1))%5; return; }
        if(ks==XK_Down || ks==XK_Up) {
            int count=static_cast<int>(g.search.results.count);
            if(count) { g.selected_place=g.selected_place<0?(ks==XK_Down?0:count-1):(g.selected_place+(ks==XK_Down?1:count-1))%count; g.search_focus=2; }
            return;
        }
        if(ks==XK_Return || ks==XK_KP_Enter || (ks==XK_space && g.search_focus!=0)) {
            if(g.search_focus==4) { g.mode=Location; g.search.clear(); }
            else if(g.search_focus==2 || g.search_focus==3) use_place(); else do_search();
            return;
        }
        if(g.search_focus!=0) return;
        bool edited=false;
        if(ctrl && (ks==XK_u || ks==XK_U)) { g.search_query.set(""); edited=true; }
        else switch(ks) {
        case XK_BackSpace: edited=g.search_query.backspace(); break;
        case XK_Delete: edited=g.search_query.del(); break;
        case XK_Left: g.search_query.left(); break;
        case XK_Right: g.search_query.right(); break;
        case XK_Home: g.search_query.home(); break;
        case XK_End: g.search_query.end(); break;
        default: if(!ctrl && n==1) edited=g.search_query.insert(buf[0]); break;
        }
        if(edited) { g.search.clear(); g.selected_place=-1; g.search_message="Enter searches; arrows select; Use fills fields."; }
        return;
    }
	if (g.mode == Location) {
		if(ctrl && (ks==XK_f || ks==XK_F)) { open_search(); return; }
		LocationForm &f = g.form;
		TextEdit<16> *t16 = f.focus == LocationForm::Lat ? &f.lat : f.focus == LocationForm::Lon ? &f.lon : nullptr;
		switch (ks) {
		case XK_Escape: g.pressed = PNone; g.mode=Normal; return;
		case XK_Return: case XK_KP_Enter: apply_location(); return;
		case XK_Tab: f.next(); return;
		case kLeftTab: f.prev(); return;
		case XK_Up: f.prev(); return;
		case XK_Down: f.next(); return;
		}
		if (f.focus == LocationForm::Units) {
			if (ks == XK_space) f.metric = !f.metric;
			return;
		}
		switch (ks) {
		case XK_BackSpace: if (t16) t16->backspace(); else f.name.backspace(); f.error = ""; return;
		case XK_Delete: if (t16) t16->del(); else f.name.del(); f.error = ""; return;
		case XK_Left: if (t16) t16->left(); else f.name.left(); return;
		case XK_Right: if (t16) t16->right(); else f.name.right(); return;
		case XK_Home: if (t16) t16->home(); else f.name.home(); return;
		case XK_End: if (t16) t16->end(); else f.name.end(); return;
		}
		if (!ctrl && n == 1) {
			bool ok = t16 ? t16->insert(buf[0]) : f.name.insert(buf[0]);
			if (ok) f.error = "";
		}
		return;
	}
	if (g.mode == About) {
		if (g.about_page >= 0) {   // Credits: Return/space/Right next page (after the last: About), Escape/Left back
			if (ks == XK_Return || ks == XK_space || ks == XK_Right) g.about_page = g.about_page + 1 < static_cast<int>(kCreditPageCount) ? g.about_page + 1 : -1;
			else if (ks == XK_Escape || ks == XK_Left) g.about_page = -1;
		} else if (ks == XK_Escape || ks == XK_Return || ks == XK_space) g.mode = Normal;
		else if (ks == XK_c || ks == XK_C) g.about_page = 0;
		return;
	}
	if (g.mode == MenuOpen) { if (ks == XK_Escape) { g.mode = Normal; g.menu_open = -1; } return; }
	if (ks == kF5) g.model.request_refresh();
	else if (ctrl && (ks == XK_l || ks == XK_L)) open_location();
	else g.dirty = false;
}

// While a fetch runs only repaint and quit; everything else is ignored.
void handle_event(XEvent &e, bool busy)
{
	switch (e.type) {
	case Expose: if (e.xexpose.count == 0) g.dirty = true; break;
	case ClientMessage:
		if (static_cast<Atom>(e.xclient.data.l[0]) == g.wm_delete) g.quit = true;
		break;
	case KeyPress:
		if (busy) {
			char b[8]; KeySym ks = NoSymbol;
			XLookupString(&e.xkey, b, sizeof b, &ks, nullptr);
			if(g.searching && ks==XK_Escape) g.search_cancel=true;
			if ((e.xkey.state & ControlMask) && (ks == XK_q || ks == XK_Q)) g.quit = true;
		} else on_key(e.xkey);
		break;
	case ButtonPress: if(busy && g.searching && s_cancel().contains(e.xbutton.x,e.xbutton.y)) g.search_cancel=true; else if (!busy) on_button_press(e.xbutton); break;
	case ButtonRelease: if (!busy) on_button_release(e.xbutton); break;
	case MotionNotify: if (!busy) on_motion(e.xmotion); break;
	}
}

void pump(bool busy)
{
	while (!g.quit && XPending(g.dpy)) {
		XEvent e;
		XNextEvent(g.dpy, &e);
		handle_event(e, busy);
	}
}

// Net::yield: keep the window alive while the network call waits; false = cancel (window closed).
bool app_yield(Net &)
{
	pump(true);
	long long t = mono_ms();
	if (g.dirty || t - g.last_spin_ms >= 250) { g.last_spin_ms = t; redraw(); }
	return !g.quit && !(g.searching && g.search_cancel);
}

void restore_cache()
{
	if(!g.cfg.cache || !g.have_cache || g.model.has_forecast()) return;
	CachedForecast previous; bool found=false;
	Status loaded=read_cached_forecast(g.cache.base,cache_key(g.cfg),now_s(),&previous,&found);
	if(loaded.ok() && found) {
		if(g.model.restore_cached(previous.forecast,previous.fetched_at,now_s())) logf(LogInfo,"restored cached forecast (stale)");
	}
	else if(!loaded.ok()) logf(LogWarn,"%s",loaded.msg);
}

void do_fetch()
{
	restore_cache();
	g.fetching = true;
	redraw();
	static char buf[64 * 1024];
	Query q;
	q.lat_e4 = g.cfg.lat_e4; q.lon_e4 = g.cfg.lon_e4; q.metric = g.cfg.metric;
	Forecast f;
	g.fo.save_forecasts=g.cfg.cache && (g.have_cache || g.fo.force_host);
	Status s = fetch_forecast_cached(g.net,g.providers,g.nprov,q,g.fo,buf,sizeof buf,&f,g.have_cache && g.cfg.cache?&g.cache.base:nullptr,g.cfg.providers);
	if(s.ok() && g.have_cache) {
		Status saved=update_forecast_cache(g.cache.base,cache_key(g.cfg),f,now_s(),g.cfg.cache);
		if(!saved.ok()) logf(LogWarn,"%s",saved.msg);
	}
	g.fetching = false;
	g.model.finish(now_s(), s, &f);
	g.dirty = true;
}

void usage(FILE *f)
{
	fputs("usage: xweather [-dhqvV] [-c file] [-display name] [-H host:port]\n"
	      "  -c  config file (default ~/.weatherrc; shared with weather(1))\n"
	      "  -d  debug: dump traffic       -q  errors only       -v  more logging\n"
	      "  -H  (testing) plain HTTP to host:port instead of the provider\n"
	      "X resources: xweather.background, .foreground, .font, .boldFont, .bigFont\n", f);
}

} // namespace

int wx::run_xweather(int argc, char **argv, PlaceLookup lookup)
{
	g.lookup=lookup;
	const char *dispname = nullptr;
	for (int i = 1; i + 1 < argc; i++)        // X toolkit style "-display NAME", removed before getopt
		if (!strcmp(argv[i], "-display")) {
			dispname = argv[i + 1];
			for (int k = i; k + 2 <= argc; k++) argv[k] = argv[k + 2];
			argc -= 2;
			break;
		}
	const char *cfgpath = nullptr, *force = nullptr;
	int verbose = 0;
	bool quiet = false, dump = false;
	int ch;
	while ((ch = getopt(argc, argv, "c:dhH:qvV")) != -1) {
		switch (ch) {
		case 'c': cfgpath = optarg; break;
		case 'd': dump = true; verbose = 3; break;
		case 'h': usage(stdout); return 0;
		case 'H': force = optarg; break;
		case 'q': quiet = true; break;
		case 'v': verbose++; break;
		case 'V': printf("xweather %s\n", WEATHER_VERSION); return 0;
		default: usage(stderr); return exit_code(Err::Usage);
		}
	}
	if (optind != argc) { usage(stderr); return exit_code(Err::Usage); }
	log_init(quiet ? LogError : verbose >= 2 ? LogDebug : verbose == 1 ? LogInfo : LogWarn, false, "xweather");

	// Settings: the same file as weather(1).
	Status ss = posix_settings_init(&g.store, cfgpath, cfgpath != nullptr);
	g.have_store = ss.ok();
	if (!ss.ok() && cfgpath) { fprintf(stderr, "xweather: %s: %s\n", err_name(ss.err), ss.msg); return exit_code(ss.err); }
	if (g.have_store) {
		static char text[4096];
		size_t n = 0;
		Status ls = g.store.base.load(g.store.base, text, sizeof text, &n);
		if (ls.ok() && n) ls = config_parse(sv(text, n), &g.cfg);
		if (!ls.ok()) {
			fprintf(stderr, "xweather: %s: %s", err_name(ls.err), ls.msg);
			if (ls.err == Err::Config && ls.detail) fprintf(stderr, " (line %d)", ls.detail);
			fputc('\n', stderr);
			return exit_code(ls.err);
		}
	}
	if (force) {
		const char *colon = strrchr(force, ':');
		if (!colon || static_cast<size_t>(colon - force) >= sizeof g.forcehost) { fprintf(stderr, "xweather: -H wants host:port\n"); return 64; }
		memcpy(g.forcehost, force, static_cast<size_t>(colon - force));
		g.forcehost[colon - force] = 0;
		g.fo.force_host = g.forcehost;
		g.fo.force_port = colon + 1;
		logf(LogWarn, "plain HTTP to %s:%s, NO TLS (testing mode)", g.forcehost, colon + 1);
	}
	g.fo.timeout_s = g.cfg.timeout_s;
	g.fo.attempts = g.cfg.attempts;
	g.fo.dump = dump;
	g.fo.contact = g.cfg.contact;
	{
		Status ps = resolve_providers(g.cfg.providers, kProviders, kNumProviders, g.providers, 8, &g.nprov);
		if (!ps.ok()) { fprintf(stderr, "xweather: %s '%s'\n", ps.msg, g.cfg.providers); return exit_code(ps.err); }
	}

	signal(SIGPIPE, SIG_IGN);
	posix_net_init(&g.net);
	tls_net_init(&g.net);
	g.net.yield = app_yield;

	g.dpy = XOpenDisplay(dispname);
	if (!g.dpy) {
		fprintf(stderr, "xweather: cannot open display '%s'\n", dispname ? dispname : (getenv("DISPLAY") ? getenv("DISPLAY") : "(DISPLAY not set)"));
		return exit_code(Err::Connect);
	}
	int scr = DefaultScreen(g.dpy);
	if (!g.th.init(g.dpy, scr)) return exit_code(Err::Config);
	layout_menus();

	g.win = XCreateSimpleWindow(g.dpy, RootWindow(g.dpy, scr), 120, 90, W, H, 0, g.th.fg, g.th.bg);
	XStoreName(g.dpy, g.win, "Weather");
	XClassHint ch2;
	char nm[] = "xweather", cl[] = "XWeather";
	ch2.res_name = nm; ch2.res_class = cl;
	XSetClassHint(g.dpy, g.win, &ch2);
	XSizeHints sh;
	memset(&sh, 0, sizeof sh);
	sh.flags = PMinSize | PMaxSize | PSize;
	sh.min_width = sh.max_width = sh.width = W;
	sh.min_height = sh.max_height = sh.height = H;
	XSetWMNormalHints(g.dpy, g.win, &sh);
	g.wm_delete = XInternAtom(g.dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(g.dpy, g.win, &g.wm_delete, 1);
	XSelectInput(g.dpy, g.win, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
	g.pm = XCreatePixmap(g.dpy, g.win, W, H, static_cast<unsigned>(DefaultDepth(g.dpy, scr)));
	XMapWindow(g.dpy, g.win);

	ModelConfig mc;
	mc.auto_refresh = g.cfg.auto_refresh;
	g.model = Model(mc, now_s());
	if(!g.fo.force_host && g.have_store) {
		Buf<1024> path; path.add(g.store.path).add(".cache");
		Status initialized=path.truncated?make(Err::Config,"cache path too long"):posix_cache_init(&g.cache,path.c_str());
		g.have_cache=initialized.ok();
		if(!initialized.ok()) logf(LogWarn,"%s",initialized.msg);
	}
    if(g.have_cache && !g.cfg.cache) {
        Status removed=remove_cached_forecast(g.cache.base);
        if(!removed.ok()) set_notice(removed.msg,30);
    }
	restore_cache();
    // Missing or empty settings are not special: the default location (Madison) is shown and fetched; Location... changes it (#198).
    if(!g.have_store) set_notice("No settings file: a changed location cannot be saved",30);
	logf(LogInfo, "xweather %s: %s, auto-refresh %s", WEATHER_VERSION, g.cfg.name[0] ? g.cfg.name : "(unnamed)", mc.auto_refresh ? "on" : "off");

	redraw();
	long long last_sec = 0;
	while (!g.quit) {
		long long t = now_s();
		if (g.mode!=Location && g.mode!=PlaceSearch && g.model.tick(t)) { do_fetch(); continue; }
		pump(false);
		if (g.quit) break;
		if (g.dirty || t != last_sec) { last_sec = t; redraw(); }
		if (XPending(g.dpy)) continue;
		struct pollfd p = { ConnectionNumber(g.dpy), POLLIN, 0 };
		poll(&p, 1, 500);
	}
	XFreePixmap(g.dpy, g.pm);
	XCloseDisplay(g.dpy);
	return 0;
}

#ifndef WEATHER_XWEATHER_LIBRARY
int main(int argc,char **argv) { return wx::run_xweather(argc,argv); }
#endif
