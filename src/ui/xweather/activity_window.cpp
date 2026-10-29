// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#include "activity_window.hpp"
#include <X11/keysym.h>
#include <ctime>
#include <cstring>

namespace cde {
bool activity_local_time(wx::ActivityTime time, char *out, size_t capacity)
{
    if (!out || !capacity) return false;
    out[0] = 0;
    if (!wx::activity_time_valid(time)) return wx::activity_utc({},out,capacity);
    time_t seconds = static_cast<time_t>(time.utc);
    tm local{};
    if (static_cast<long long>(seconds) != time.utc || !localtime_r(&seconds,&local) ||
            local.tm_year < -1900 || local.tm_year > 8099)
        return wx::activity_utc({},out,capacity);
    if (capacity < 20) return false;
    if (std::strftime(out,capacity,"%Y-%m-%d %H:%M:%S",&local) == 19) return true;
    out[0] = 0;
    return false;
}
static bool tabular_lining_digits(XFontStruct *font)
{
    if (!font || font->min_byte1 || font->max_byte1 ||
            font->min_char_or_byte2 > '0' || font->max_char_or_byte2 < '9') return false;
    int direction=0,ascent=0,descent=0; XCharStruct capital{};
    XTextExtents(font,"H",1,&direction,&ascent,&descent,&capital);
    const int width=XTextWidth(font,"0",1);
    if (width<=0 || capital.ascent<=0) return false;
    for (char digit='0';digit<='9';++digit) {
        XCharStruct glyph{};
        XTextExtents(font,&digit,1,&direction,&ascent,&descent,&glyph);
        if (glyph.width!=width || glyph.ascent!=capital.ascent || glyph.descent!=capital.descent) return false;
    }
    return true;
}
ActivityLayout activity_layout(int height, int text_ascent, int text_descent, int number_ascent, int number_descent)
{
    int ascent=text_ascent>number_ascent?text_ascent:number_ascent;
    int descent=text_descent>number_descent?text_descent:number_descent;
    // X font metrics are signed 16-bit values. Keep malformed metrics bounded.
    if(ascent<0) ascent=0;
    if(descent<0) descent=0;
    if(ascent>32767) ascent=32767;
    if(descent>32767) descent=32767;
    const int row_height=ascent+descent+4;
    const int available=height>128?height-128:0;
    const int rows=available/row_height;
    return {ascent,row_height,static_cast<size_t>(rows>0?rows:1)};
}
size_t ActivityWindow::page() const
{
    return activity_layout(height_,theme_.normal->ascent,theme_.normal->descent,digits_->ascent,digits_->descent).rows;
}
size_t ActivityWindow::last_start() const
{
    return history_ && history_->size() > page() ? history_->size() - page() : 0;
}
Rect ActivityWindow::button_rect(int index) const
{
    const int width = (width_ - 48) / 4;
    return {12 + index * (width + 8),height_ - 42,width,28};
}
bool ActivityWindow::show(Theme &theme, const wx::ActivityLog &history)
{
    if (window_) { XMapRaised(theme_.dpy,window_); return true; }
    if (!theme.dpy || !theme.normal || !theme.bold) return false;
    theme_ = theme;
    theme_.gc = XCreateGC(theme_.dpy,RootWindow(theme_.dpy,theme_.screen),0,nullptr);
    digits_ = XLoadQueryFont(theme_.dpy,"fixed");
    if (!theme_.gc || !tabular_lining_digits(digits_)) { close(); return false; }
    const int screen_w = DisplayWidth(theme_.dpy,theme_.screen);
    const int screen_h = DisplayHeight(theme_.dpy,theme_.screen);
    width_ = screen_w > 632 ? 600 : (screen_w > 32 ? screen_w - 32 : 1);
    height_ = screen_h > 364 ? 300 : (screen_h > 64 ? screen_h - 64 : 1);
    const ActivityLayout layout=activity_layout(height_,theme_.normal->ascent,theme_.normal->descent,digits_->ascent,digits_->descent);
    row_height_=layout.row_height; baseline_ascent_=layout.ascent;
    if(height_-128<row_height_) { close(); return false; }
    window_ = XCreateSimpleWindow(theme_.dpy,RootWindow(theme_.dpy,theme_.screen),
        (screen_w-width_)/2,(screen_h-height_)/2,static_cast<unsigned>(width_),static_cast<unsigned>(height_),1,theme_.fg,theme_.bg);
    if (!window_) { close(); return false; }
    history_ = &history;
    XStoreName(theme_.dpy,window_,"Weather Activity Log");
    char name[]="xweather-activity", cls[]="XWeather";
    XClassHint hint{name,cls}; XSetClassHint(theme_.dpy,window_,&hint);
    XSizeHints sizes{}; sizes.flags=PMinSize|PSize;
    sizes.min_width=320; sizes.min_height=row_height_+128>180?row_height_+128:180;
    sizes.width=width_; sizes.height=height_;
    XSetWMNormalHints(theme_.dpy,window_,&sizes);
    protocols_=XInternAtom(theme_.dpy,"WM_PROTOCOLS",False);
    delete_=XInternAtom(theme_.dpy,"WM_DELETE_WINDOW",False);
    XSetWMProtocols(theme_.dpy,window_,&delete_,1);
    XSelectInput(theme_.dpy,window_,ExposureMask|StructureNotifyMask|KeyPressMask|ButtonPressMask|ButtonReleaseMask);
    backing_=XCreatePixmap(theme_.dpy,window_,static_cast<unsigned>(width_),static_cast<unsigned>(height_),
        static_cast<unsigned>(DefaultDepth(theme_.dpy,theme_.screen)));
    first_=last_start(); follow_=true; dirty_=true; pressed_=-1; focus_=2;
    XMapWindow(theme_.dpy,window_); paint();
    return true;
}
void ActivityWindow::close()
{
    if (theme_.dpy) {
        if (backing_) XFreePixmap(theme_.dpy,backing_);
        if (window_) XDestroyWindow(theme_.dpy,window_);
        if (digits_) XFreeFont(theme_.dpy,digits_);
        if (theme_.gc) XFreeGC(theme_.dpy,theme_.gc);
    }
    window_=0; backing_=0; digits_=nullptr; history_=nullptr; theme_=Theme{};
}
void ActivityWindow::changed(bool evicted)
{
    if (!window_) return;
    if (evicted && !follow_ && first_) --first_;
    if (follow_ || first_ > last_start()) first_=last_start();
    dirty_=true;
}
void ActivityWindow::scroll(int rows)
{
    long position=static_cast<long>(first_)+rows;
    if (position<0) position=0;
    if (position>static_cast<long>(last_start())) position=static_cast<long>(last_start());
    first_=static_cast<size_t>(position); follow_=first_==last_start(); dirty_=true;
}
void ActivityWindow::activate(int index)
{
    if (index==0) scroll(-static_cast<int>(page()));
    else if (index==1) scroll(static_cast<int>(page()));
    else if (index==2) { first_=last_start(); follow_=true; dirty_=true; }
    else if (index==3) close();
}
void ActivityWindow::render()
{
    fill(theme_,backing_,{0,0,width_,height_},theme_.bg);
    text(theme_,backing_,theme_.bold,12,22,"Activity Log - current session",theme_.fg);
    if (width_<200 || height_<150 || height_-128<row_height_) return;
    const int number_width=XTextWidth(digits_,"0",1)*19;
    const int level_x=20+number_width+12, message_x=level_x+54;
    text(theme_,backing_,theme_.bold,20,40,"Local time",theme_.fg);
    text(theme_,backing_,theme_.bold,level_x,40,"Level",theme_.fg);
    text(theme_,backing_,theme_.bold,message_x,40,"Activity",theme_.fg);
    Rect well{12,46,width_-24,height_-120};
    fill(theme_,backing_,well,WhitePixel(theme_.dpy,theme_.screen)); shadow(theme_,backing_,well,2,false);
    XRectangle clip{16,50,static_cast<unsigned short>(width_-32),static_cast<unsigned short>(height_-128)};
    XSetClipRectangles(theme_.dpy,theme_.gc,0,0,&clip,1,Unsorted);
    const unsigned long ink=BlackPixel(theme_.dpy,theme_.screen);
    for (size_t i=0;i<page();++i) {
        const wx::ActivityEntry *entry=history_->at(first_+i);
        if (!entry) break;
        const int baseline=54+baseline_ascent_+static_cast<int>(i)*row_height_;
        char time[32]; activity_local_time(entry->time,time,sizeof time);
        text(theme_,backing_,digits_,20,baseline,time,ink);
        text(theme_,backing_,theme_.normal,level_x,baseline,
            entry->severity==wx::ActivitySeverity::Warning ? "WARN" : "INFO",ink);
        char message[192]; fit_text(theme_.normal,entry->message,width_-message_x-20,message,sizeof message);
        text(theme_,backing_,theme_.normal,message_x,baseline,message,ink);
    }
    XSetClipMask(theme_.dpy,theme_.gc,None);
    wx::Buf<80> footer;
    if (!history_->size()) footer.add("No activity yet");
    else {
        size_t end=first_+page(); if(end>history_->size()) end=history_->size();
        footer.add("Events ").add_int(static_cast<long>(first_+1)).add('-').add_int(static_cast<long>(end));
        footer.add(" of ").add_int(static_cast<long>(history_->size()));
    }
    text(theme_,backing_,digits_,12,height_-62,footer.view(),theme_.fg);
    const char *labels[]={"Up","Down","Latest","Close"};
    for(int i=0;i<4;++i) button(theme_,backing_,button_rect(i),labels[i],pressed_==i,focus_==i);
}
void ActivityWindow::paint()
{
    if (!window_ || !backing_) return;
    if (dirty_) { render(); dirty_=false; }
    XCopyArea(theme_.dpy,backing_,window_,theme_.gc,0,0,static_cast<unsigned>(width_),static_cast<unsigned>(height_),0,0);
    XFlush(theme_.dpy);
}
bool ActivityWindow::handle(XEvent &event)
{
    if (!window_ || event.xany.window!=window_) return false;
    switch(event.type) {
    case Expose: if(!event.xexpose.count) paint(); return true;
    case DestroyNotify: window_=0; close(); return true;
    case ClientMessage:
        if(event.xclient.message_type==protocols_ && event.xclient.format==32 &&
                static_cast<Atom>(event.xclient.data.l[0])==delete_) close();
        return true;
    case ConfigureNotify:
        if(event.xconfigure.width!=width_ || event.xconfigure.height!=height_) {
            width_=event.xconfigure.width>0?event.xconfigure.width:1;
            height_=event.xconfigure.height>0?event.xconfigure.height:1;
            XFreePixmap(theme_.dpy,backing_);
            backing_=XCreatePixmap(theme_.dpy,window_,static_cast<unsigned>(width_),static_cast<unsigned>(height_),
                static_cast<unsigned>(DefaultDepth(theme_.dpy,theme_.screen)));
            changed(); paint();
        }
        return true;
    case KeyPress: {
        char bytes[8]; KeySym symbol=NoSymbol;
        XLookupString(&event.xkey,bytes,sizeof bytes,&symbol,nullptr);
        if(symbol==XK_Escape) { close(); return true; }
        if(symbol==XK_Up) scroll(-1);
        else if(symbol==XK_Down) scroll(1);
        else if(symbol==XK_Page_Up) scroll(-static_cast<int>(page()));
        else if(symbol==XK_Page_Down) scroll(static_cast<int>(page()));
        else if(symbol==XK_Home) { first_=0; follow_=last_start()==0; dirty_=true; }
        else if(symbol==XK_End) activate(2);
        else if(symbol==XK_Tab || symbol==XK_ISO_Left_Tab) {
            const bool back=symbol==XK_ISO_Left_Tab || (event.xkey.state&ShiftMask);
            focus_=(focus_+(back?3:1))%4; dirty_=true;
        } else if(symbol==XK_Return || symbol==XK_space) activate(focus_);
        paint(); return true;
    }
    case ButtonPress:
        if(event.xbutton.button==Button4) scroll(-3);
        else if(event.xbutton.button==Button5) scroll(3);
        else if(event.xbutton.button==Button1 && width_>=200 && height_>=150)
            for(int i=0;i<4;++i) if(button_rect(i).contains(event.xbutton.x,event.xbutton.y)) { pressed_=focus_=i; dirty_=true; }
        paint(); return true;
    case ButtonRelease: {
        const int pressed=pressed_; pressed_=-1; dirty_=true;
        if(event.xbutton.button==Button1 && pressed>=0 && button_rect(pressed).contains(event.xbutton.x,event.xbutton.y)) activate(pressed);
        paint(); return true;
    }
    }
    return true;
}
} // namespace cde
