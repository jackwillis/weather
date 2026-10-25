// Native Toolbox window. Network initialization and terminal shutdown occur once.
#include "app/window_actions.hpp"
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
const unsigned char *pascal_text(const char *s) {
	static unsigned char p[256]; size_t n=strlen(s); if(n>255) n=255;
	p[0]=static_cast<unsigned char>(n); memcpy(p+1,s,n); return p;
}
Rect box(int x, int y, int w, int h) { return Rect{short(y), short(x), short(y+h), short(x+w)}; }
void color(Rgb c) { RGBColor rgb{static_cast<unsigned short>(c.r*257), static_cast<unsigned short>(c.g*257), static_cast<unsigned short>(c.b*257)}; RGBForeColor(&rgb); }
struct Canvas {
	void rect(int x,int y,int w,int h,Rgb c) { color(c); Rect r=box(x,y,w,h); PaintRect(&r); }
	void disc(int x,int y,int r,Rgb c) { color(c); Rect b=box(x-r,y-r,2*r,2*r); PaintOval(&b); }
	void line(int x,int y,int xx,int yy,int width,Rgb c) {
		color(c); PenSize(width,width); MoveTo(x,y); LineTo(xx,yy); PenNormal();
		disc(x,y,width/2,c); disc(xx,yy,width/2,c);
	}
	void poly(const ArtPt *p,int n,Rgb c) {
		if (!n) return;
		PolyHandle h=OpenPoly(); if (!h) return;
		MoveTo(p[0].x,p[0].y); for(int i=1;i<n;i++) LineTo(p[i].x,p[i].y);
		LineTo(p[0].x,p[0].y); ClosePoly(); color(c); PaintPoly(h); KillPoly(h);
	}
};
void text(int x,int y,const char *s,int size=12,int width=308) {
	color({0,0,0}); TextFont(0); TextSize(size); TextFace(0);
	int n=static_cast<int>(strlen(s));
	while(n && TextWidth(const_cast<char *>(s),0,n)>width) --n;
	MoveTo(x,y); DrawText(const_cast<char *>(s),0,n);
}
void draw() {
	SetPort(window); Canvas c; c.rect(0,0,340,286,{221,221,221});
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
	Buf<256> status; model.status_line(status,net.now(net)); text(16,214,status.c_str(),10);
	text(16,263,cfg.auto_refresh?"Automatic updates on":"Automatic updates off",9,205);
	Rect r=box(236,246,88,24); color({0,0,0}); FrameRoundRect(&r,8,8); text(248,263,"Refresh",12,70);
	if(actions.about) {
		c.rect(26,53,288,158,{238,238,238}); Rect a=box(26,53,288,158); color({0,0,0}); FrameRect(&a);
		text(40,78,"Weather for Mac OS 9",12,260);
		text(40,103,"Weather data: Open-Meteo",10,260);
		text(40,125,"TLS seed trouble? Quit Weather apps.",10,260);
		text(40,143,"Follow docs/OS9-SEEDS.md recovery.",10,260);
		text(40,185,"Press Return or Escape to close.",10,260);
	}
}
void command(WindowAction a) { actions.apply(a,cfg,model,net.now(net)); draw(); }
void menu(long selection) {
	int id=selection>>16,item=selection&65535;
	if(id==128 && item==1) command(WindowAction::About);
	if(id==129 && item==1) command(WindowAction::Refresh);
	if(id==129 && item==3) command(WindowAction::Quit);
	if(id==130 && item==1) command(WindowAction::ToggleAuto);
	if(id==130 && item==2) command(WindowAction::Metric);
	if(id==130 && item==3) command(WindowAction::Imperial);
	HiliteMenu(0);
}
bool pump(Net &) {
	MenuHandle file=GetMenuHandle(129), options=GetMenuHandle(130);
	bool fetching=model.phase()==Phase::Fetching;
	if(fetching) { DisableItem(file,1); DisableItem(options,0); }
	else { EnableItem(file,1); EnableItem(options,0); }
	EventRecord e;
	for(int i=0;i<8 && WaitNextEvent(everyEvent,&e,0,nullptr);i++) {
		if(e.what==updateEvt) { WindowPtr w=reinterpret_cast<WindowPtr>(e.message); BeginUpdate(w); if(w==window) draw(); EndUpdate(w); }
		else if(e.what==activateEvt || e.what==osEvt) draw();
		else if(e.what==keyDown) {
			char ch=e.message&charCodeMask;
			if(e.modifiers&cmdKey) menu(MenuKey(ch));
			else if(ch==13 || ch==27) command(WindowAction::DismissAbout);
		} else if(e.what==mouseDown) {
			WindowPtr w=nullptr; short part=FindWindow(e.where,&w);
			if(part==inMenuBar) menu(MenuSelect(e.where));
			else if(w==window && part==inGoAway && TrackGoAway(w,e.where)) command(WindowAction::Quit);
			else if(w==window && part==inDrag) { Rect bounds=box(4,24,1000,700); DragWindow(w,e.where,&bounds); }
			else if(w==window && part==inContent) {
				SelectWindow(w); SetPort(w); Point p=e.where; GlobalToLocal(&p);
				if(p.h>=236 && p.h<324 && p.v>=246 && p.v<270) command(WindowAction::Refresh);
			}
		}
	}
	return !actions.quit;
}
}
int main() {
	InitGraf(&qd.thePort); InitFonts(); InitWindows(); InitMenus(); TEInit(); InitDialogs(nullptr); InitCursor();
	Rect bounds=box(80,60,340,286);
	window=NewCWindow(nullptr,&bounds,pascal_text("Weather"),true,documentProc,reinterpret_cast<WindowPtr>(-1L),true,0);
	if(!window) return 1;
	MenuHandle apple=NewMenu(128,pascal_text("\024")); AppendMenu(apple,pascal_text("About Weather")); InsertMenu(apple,0);
	MenuHandle file=NewMenu(129,pascal_text("File")); AppendMenu(file,pascal_text("Refresh/R;(-;Quit/Q")); InsertMenu(file,0);
	MenuHandle options=NewMenu(130,pascal_text("Options")); AppendMenu(options,pascal_text("Automatic updates;Metric;Imperial")); InsertMenu(options,0); DrawMenuBar();
	os9_net_init(&net); net.yield=pump; tls_net_init(&net); draw();
	static char response[65536];
	long long painted_at=-1;
	while(!actions.quit) {
		pump(net); if(actions.quit) break;
		long long now=net.now(net);
		if(now!=painted_at) { draw(); painted_at=now; }
		if(model.tick(net.now(net))) {
			draw(); Query q; q.lat_e4=cfg.lat_e4; q.lon_e4=cfg.lon_e4; q.metric=cfg.metric;
			FetchOpts opts; opts.timeout_s=cfg.timeout_s; opts.attempts=cfg.attempts; opts.contact=cfg.contact;
			const Provider *providers[8]; size_t count=0; Forecast f;
			wx::Status s=resolve_providers(cfg.providers,kProviders,kNumProviders,providers,8,&count);
			if(s.ok()) s=fetch_forecast(net,providers,count,q,opts,response,sizeof response,&f);
			model.finish(net.now(net),s,&f); draw();
		}
		CheckItem(options,1,cfg.auto_refresh); CheckItem(options,2,cfg.metric); CheckItem(options,3,!cfg.metric);
		// Sleep cooperatively without consuming an unhandled event.
		long finalTicks; Delay(1,&finalTicks);
	}
	os9_net_shutdown(); DisposeWindow(window); return 0;
}
