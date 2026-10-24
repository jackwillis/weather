// xweather: current conditions in a CDE-look X11 window. One window; menus and dialogs are
// drawn as overlays inside it. All refresh behaviour comes from app/Model; the (blocking)
// fetch keeps the window alive through Net::yield, and can be cancelled by closing it.
#include "app/locform.hpp"
#include "app/model.hpp"
#include "cde.hpp"
#include "core/config.hpp"
#include "core/fetch.hpp"
#include "core/log.hpp"
#include "core/openmeteo.hpp"
#include "core/settings.hpp"
#include "core/version.hpp"
#include "platform/posix/net_posix.hpp"
#include "platform/posix/settings_posix.hpp"
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

enum Mode { Normal, MenuOpen, Location, About };
enum Cmd { CmdRefresh = 1, CmdLocation, CmdExit, CmdAuto, CmdImperial, CmdMetric, CmdAbout };
enum Press { PNone = 0, PRefresh, PLocation, POk, PCancel, PAboutOk };

struct MenuItem { const char *label, *accel; int cmd; int kind; };   // kind: 0 plain, 1 check, 2 radio
const MenuItem kFile[] = {{"Refresh", "F5", CmdRefresh, 0}, {"Location...", "Ctrl+L", CmdLocation, 0}, {"Exit", "Ctrl+Q", CmdExit, 0}};
const MenuItem kOptions[] = {{"Auto-refresh", "", CmdAuto, 1}, {"Imperial (F, mph)", "", CmdImperial, 2}, {"Metric (C, km/h)", "", CmdMetric, 2}};
const MenuItem kHelp[] = {{"About xweather...", "", CmdAbout, 0}};
struct Menu { const char *title; const MenuItem *items; int n; Rect bar; Rect drop; };

const Provider *const kAll[] = { &kOpenMeteo };

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
	FetchOpts fo;
	char forcehost[256] = "";
	const Provider *providers[8];
	size_t nprov = 0;
	Menu menus[3];
	Mode mode = Normal;
	int menu_open = -1, menu_hover = -1;
	bool menu_by_press = false;   // opened by a click (a second click on its title closes it), not by hovering
	Press pressed = PNone;
	LocationForm form;
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
Rect d_units() { Rect p = d_panel(); return Rect{p.x + 16, p.y + 148, 300, 22}; }
Rect d_ok() { Rect p = d_panel(); return Rect{p.x + 60, p.y + 200, 96, 26}; }
Rect d_cancel() { Rect p = d_panel(); return Rect{p.x + 184, p.y + 200, 96, 26}; }
Rect a_panel() { return Rect{70, 60, 300, 170}; }
Rect a_ok() { Rect p = a_panel(); return Rect{p.x + 102, p.y + 130, 96, 26}; }

bool menu_checked(const MenuItem &m)
{
	switch (m.cmd) {
	case CmdAuto: return g.cfg.auto_refresh;
	case CmdImperial: return !g.cfg.metric;
	case CmdMetric: return g.cfg.metric;
	}
	return false;
}

void layout_menus()
{
	static const struct { const char *t; const MenuItem *it; int n; } defs[3] = {
		{"File", kFile, 3}, {"Options", kOptions, 3}, {"Help", kHelp, 1}};
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
	cde::text(t, d, t.big, x0, 108, temp.view(), t.fg);
	cde::text(t, d, t.bold, x0, 132, wmo_text(f.code), t.fg);
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
	Buf<48> obs; obs.add("Observed ");
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
		static const char *dots[] = {"", ".", "..", "..."};
		st.add("Fetching").add(dots[(mono_ms() / 300) % 4]);
		if (g.model.has_forecast()) st.add(" (showing data from ").add_int(0).add(" min ago)");
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
	cde::button(t, d, d_ok(), "OK", g.pressed == POk, true);
	cde::button(t, d, d_cancel(), "Cancel", g.pressed == PCancel, false);
}

void draw_about(Drawable d)
{
	cde::Theme &t = g.th;
	Rect p = a_panel();
	draw_panel(d, p, "About xweather");
	Buf<48> v; v.add("xweather ").add(WEATHER_VERSION);
	cde::text(t, d, t.bold, p.x + 16, p.y + 52, v.view(), t.fg);
	cde::text(t, d, t.normal, p.x + 16, p.y + 72, "Weather for old Unix machines.", t.fg);
	cde::text(t, d, t.normal, p.x + 16, p.y + 90, "Data from Open-Meteo.com.", t.fg);
	cde::text(t, d, t.normal, p.x + 16, p.y + 108, "BSD 2-clause license.", t.fg);
	cde::button(t, d, a_ok(), "OK", g.pressed == PAboutOk, true);
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
	Config c = g.cfg;
	if (!g.form.read(&c).ok()) { g.dirty = true; return; }
	g.cfg = c;
	if (g.have_store) {
		Status s = settings_save(g.store.base, g.cfg);
		if (!s.ok()) {
			char m[160];
			snprintf(m, sizeof m, "Could not save settings: %s", s.msg);
			set_notice(m);
		}
	} else {
		set_notice("Location not saved: no HOME and no -c file");
	}
	g.model.location_changed();
	g.mode = Normal;
	g.dirty = true;
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
	case CmdAbout: g.mode = About; break;
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
	size_t pos = 0;
	while (pos < n && cde::text_width(g.th.normal, sv(s, pos + 1)) - cde::text_width(g.th.normal, sv(s + pos, 1)) / 2 < rel) pos++;
	if (t16) t16->set_cursor(pos); else g.form.name.set_cursor(pos);
}

void on_button_press(const XButtonEvent &e)
{
	if (e.button != Button1) return;
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
	if (g.mode == Location) {
		for (int i = 0; i < 3; i++)
			if (d_field(i).contains(e.x, e.y)) { g.form.focus = i; field_click(i, e.x); return; }
		if (d_units().contains(e.x, e.y)) { g.form.focus = LocationForm::Units; g.form.metric = !g.form.metric; return; }
		if (d_ok().contains(e.x, e.y)) g.pressed = POk;
		else if (d_cancel().contains(e.x, e.y)) g.pressed = PCancel;
		return;
	}
	if (g.mode == About) { if (a_ok().contains(e.x, e.y)) g.pressed = PAboutOk; return; }
	if (r_refresh().contains(e.x, e.y)) g.pressed = PRefresh;
	else if (r_locbtn().contains(e.x, e.y)) g.pressed = PLocation;
}

void on_button_release(const XButtonEvent &e)
{
	Press p = g.pressed;
	g.pressed = PNone;
	g.dirty = true;
	if (p == PNone) return;
	Rect r = p == PRefresh ? r_refresh() : p == PLocation ? r_locbtn() : p == POk ? d_ok() : p == PCancel ? d_cancel() : a_ok();
	if (!r.contains(e.x, e.y)) return;
	switch (p) {
	case PRefresh: g.model.request_refresh(); break;
	case PLocation: open_location(); break;
	case POk: apply_location(); break;
	case PCancel: case PAboutOk: g.mode = Normal; break;
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
	if (g.mode == Location) {
		LocationForm &f = g.form;
		TextEdit<16> *t16 = f.focus == LocationForm::Lat ? &f.lat : f.focus == LocationForm::Lon ? &f.lon : nullptr;
		switch (ks) {
		case XK_Escape: g.mode = Normal; return;
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
	if (g.mode == About) { if (ks == XK_Escape || ks == XK_Return || ks == XK_space) g.mode = Normal; return; }
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
			if ((e.xkey.state & ControlMask) && (ks == XK_q || ks == XK_Q)) g.quit = true;
		} else on_key(e.xkey);
		break;
	case ButtonPress: if (!busy) on_button_press(e.xbutton); break;
	case ButtonRelease: if (!busy) on_button_release(e.xbutton); break;
	case MotionNotify: if (!busy) on_motion(e.xmotion); break;
	}
}

void pump(bool busy)
{
	while (XPending(g.dpy)) {
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
	return !g.quit;
}

void do_fetch()
{
	g.fetching = true;
	redraw();
	static char buf[64 * 1024];
	Query q;
	q.lat_e4 = g.cfg.lat_e4; q.lon_e4 = g.cfg.lon_e4; q.metric = g.cfg.metric;
	Forecast f;
	Status s = fetch_forecast(g.net, g.providers, g.nprov, q, g.fo, buf, sizeof buf, &f);
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

int main(int argc, char **argv)
{
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
	{
		Status ps = resolve_providers(g.cfg.providers, kAll, 1, g.providers, 8, &g.nprov);
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
	logf(LogInfo, "xweather %s: %s, auto-refresh %s", WEATHER_VERSION, g.cfg.name[0] ? g.cfg.name : "(unnamed)", mc.auto_refresh ? "on" : "off");

	redraw();
	long long last_sec = 0;
	while (!g.quit) {
		long long t = now_s();
		if (g.model.tick(t)) { do_fetch(); continue; }
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
