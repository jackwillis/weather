// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#include "ui/xweather/activity_window.hpp"
#include <X11/keysym.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <initializer_list>

static int checks, failures;
static void check(bool ok, const char *name) { ++checks; if (!ok) { ++failures; std::printf("FAIL activity window: %s\n", name); } }
static unsigned long pixels(Display *d, Window w, bool rows=false)
{
    XSync(d, False);
    XWindowAttributes a{};
    if (!XGetWindowAttributes(d, w, &a)) return 0;
    const int x0=rows?16:0, y0=rows?50:0;
    const int width=rows?a.width-32:a.width, height=rows?a.height-128:a.height;
    XImage *im = XGetImage(d, w, x0, y0, width, height, AllPlanes, ZPixmap);
    if (!im) return 0;
    unsigned long hash = 5381;
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
        hash = (hash * 33) ^ XGetPixel(im, x, y);
    XDestroyImage(im);
    return hash;
}
static void key(Display *d, cde::ActivityWindow &view, KeySym symbol)
{
    XEvent e{}; e.xkey.type = KeyPress; e.xkey.display = d; e.xkey.window = view.window();
    e.xkey.keycode = XKeysymToKeycode(d, symbol); view.handle(e);
}
int main()
{
    for (int height : {180,292,300,420}) for (int ascent : {3,16,30}) for (int descent : {0,4,8}) {
        const cde::ActivityLayout layout=cde::activity_layout(height,ascent,descent,11,2);
        const int maximum_ascent=ascent>11?ascent:11, maximum_descent=descent>2?descent:2;
        check(54+layout.ascent-maximum_ascent>=50,"mixed-font first row fits clip");
        check(54+layout.ascent+static_cast<int>(layout.rows-1)*layout.row_height+maximum_descent<=height-78,
            "last visible row fits clip at every tested size");
    }
    setenv("TZ", "UTC0", 1); tzset();
    char stamp[32];
    check(cde::activity_local_time({1735689600LL,true},stamp,sizeof stamp) && !std::strcmp(stamp,"2025-01-01 00:00:00"), "UTC timezone conversion");
    setenv("TZ", "EST5", 1); tzset();
    check(cde::activity_local_time({1735689600LL,true},stamp,sizeof stamp) && !std::strcmp(stamp,"2024-12-31 19:00:00"), "local date crosses midnight");
    check(cde::activity_local_time({},stamp,sizeof stamp) && !std::strcmp(stamp,"TIME-UNKNOWN"), "unknown clock remains unknown");
    check(!cde::activity_local_time({1735689600LL,true},stamp,2) && stamp[0]==0, "small local-time output refused");
    setenv("TZ", "UTC0", 1); tzset();
    Display *d = XOpenDisplay(nullptr);
    if (!d) { std::puts("activity window: no test display"); return 1; }
    cde::Theme theme;
    if (!theme.init(d,DefaultScreen(d))) return 1;
    wx::ActivityLog history;
    for (size_t i=0;i<128;++i) history.record(wx::ActivityEvent::RefreshSucceeded,{1735689600LL+static_cast<long long>(i),true});
    cde::ActivityWindow view;
    check(view.show(theme,history), "opens viewer");
    Window window=view.window(), root=0,parent=0,*children=nullptr; unsigned count=0;
    check(window && XQueryTree(d,window,&root,&parent,&children,&count) && parent==DefaultRootWindow(d), "ordinary top-level window");
    if(children) XFree(children);
    XClassHint cls{};
    check(XGetClassHint(d,window,&cls) && !std::strcmp(cls.res_name,"xweather-activity"), "window manager class");
    if(cls.res_name) XFree(cls.res_name);
    if(cls.res_class) XFree(cls.res_class);
    const unsigned long tail=pixels(d,window);
    check(tail!=0,"viewer paints actual pixels");
    key(d,view,XK_Home); check(pixels(d,window)!=tail,"Home scrolls to oldest events");
    key(d,view,XK_End); check(pixels(d,window)==tail,"End restores newest events");
    XClearArea(d,window,0,0,0,0,False);
    XEvent expose{}; expose.xexpose.type=Expose; expose.xexpose.window=window;
    check(view.handle(expose) && pixels(d,window)==tail,"expose restores cached rendering");
    XEvent foreign=expose; foreign.xexpose.window=DefaultRootWindow(d);
    check(!view.handle(foreign),"foreign events left for main window");
    key(d,view,XK_Home); key(d,view,XK_Page_Down);
    const unsigned long held=pixels(d,window,true);
    history.record(wx::ActivityEvent::RefreshSucceeded,{1735689728LL,true});
    view.changed(true); view.paint();
    check(pixels(d,window,true)==held,"reading position survives ring eviction");
    key(d,view,XK_End);
    const unsigned long new_tail=pixels(d,window);
    history.record(wx::ActivityEvent::RefreshFailed,{1735689800LL,true},wx::make(wx::Err::Timeout,"PRIVATE RAW TEXT"));
    view.changed(true); view.paint();
    check(pixels(d,window)!=new_tail,"new events repaint while following the tail");
    const size_t retained=history.size();
    XEvent press{}; press.xbutton.type=ButtonPress; press.xbutton.window=window;
    press.xbutton.button=Button1; press.xbutton.x=500; press.xbutton.y=270;
    view.handle(press); press.xbutton.type=ButtonRelease; press.xbutton.x=5; press.xbutton.y=5; view.handle(press);
    check(view.window()==window,"dragging off Close cancels the action");
    XEvent close{}; close.xclient.type=ClientMessage; close.xclient.window=window;
    close.xclient.message_type=XInternAtom(d,"WM_PROTOCOLS",False); close.xclient.format=32;
    close.xclient.data.l[0]=static_cast<long>(XInternAtom(d,"WM_DELETE_WINDOW",False));
    check(view.handle(close) && view.window()==0,"window-manager close closes only viewer");
    check(history.size()==retained,"closing viewer retains session history");
    check(view.show(theme,history),"viewer reopens");
    key(d,view,XK_Escape); check(!view.window(),"Escape closes viewer");
    check(view.show(theme,history),"viewer opens for keyboard button navigation");
    key(d,view,XK_Tab); key(d,view,XK_Return);
    check(!view.window(),"Tab and Return activate Close");
    history.clear(); check(view.show(theme,history),"empty session opens safely");
    view.close();
    XFreeGC(d,theme.gc);
    if(theme.big!=theme.bold && theme.big!=theme.normal) XFreeFont(d,theme.big);
    if(theme.bold!=theme.normal) XFreeFont(d,theme.bold);
    XFreeFont(d,theme.normal); XCloseDisplay(d);
    std::printf("activity window: %d checks, %d failed\n",checks,failures);
    return failures ? 1 : 0;
}
