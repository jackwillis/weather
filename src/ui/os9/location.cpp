#include "location.hpp"
#include <Events.h>
#include <cstring>

namespace wx {
namespace {
struct DialogPort {
    GrafPtr old;
    explicit DialogPort(DialogPtr dialog) { GetPort(&old); SetPort(reinterpret_cast<WindowPtr>(dialog)); }
    ~DialogPort() { SetPort(old); }
};
void select(DialogPtr d,int item) { DialogPort port(d); SelectDialogItemText(d,item,0,32767); }
Handle item_handle(DialogPtr d,int item) {
    short type; Handle h=nullptr; Rect r;
    GetDialogItem(d,item,&type,&h,&r); return h;
}
void set_text(DialogPtr d,int item,sv text) {
    DialogPort port(d);
    unsigned char p[256]; size_t n=text.size()<255?text.size():255;
    p[0]=static_cast<unsigned char>(n); std::memcpy(p+1,text.data(),n);
    SetDialogItemText(item_handle(d,item),p);
}
void check(DialogPtr d,int item,bool on) { DialogPort port(d); SetControlValue(reinterpret_cast<ControlHandle>(item_handle(d,item)),on); }
bool checked(DialogPtr d,int item) { DialogPort port(d); return GetControlValue(reinterpret_cast<ControlHandle>(item_handle(d,item)))!=0; }
}
void LocationDialog::message(const char *text) { set_text(dialog_,11,text); draw(); }
Status LocationDialog::open(const Config &cfg,const Model &model) {
    if(!editor_.open(cfg,model)) return make(Err::Config,"wait for the update to finish before changing location");
    // Do not fall through to a same-numbered system resource if delivery lost
    // the application's resource fork. Only our current resource file qualifies.
    if(!Get1Resource('DLOG',200) || !Get1Resource('DITL',200)) {
        editor_.cancel(); return make(Err::Io,"Location dialog resources are missing");
    }
    GetPort(&prior_);
    dialog_=GetNewDialog(200,nullptr,reinterpret_cast<WindowPtr>(-1L));
    if(!dialog_) { SetPort(prior_); editor_.cancel(); return make(Err::Io,"cannot open the Location dialog"); }
    set_text(dialog_,3,editor_.form.lat.view()); set_text(dialog_,4,editor_.form.lon.view()); set_text(dialog_,5,editor_.form.name.view());
    check(dialog_,6,cfg.metric); check(dialog_,7,cfg.auto_refresh);
    focus_=0; select(dialog_,3);
    ShowWindow(window()); SelectWindow(window()); draw(); SetPort(prior_); return okay();
}
void LocationDialog::cancel() {
    if(dialog_) { SetPort(prior_); DisposeDialog(dialog_); dialog_=nullptr; SetPort(prior_); }
    editor_.cancel();
}
void LocationDialog::draw() {
    if(!dialog_) return;
    GrafPtr saved; GetPort(&saved); SetPort(window());
    DrawDialog(dialog_);
    short type; Handle h; Rect button; GetDialogItem(dialog_,1,&type,&h,&button);
    InsetRect(&button,-4,-4); PenSize(3,3); FrameRoundRect(&button,16,16); PenNormal();
    SetPort(saved);
}
void LocationDialog::set_field(int field,sv text) {
    if(dialog_ && field>=0 && field<3) set_text(dialog_,field+3,text);
}
bool LocationDialog::read() {
    DialogPort port(dialog_);
    for(int i=0;i<3;++i) {
        unsigned char text[256]; GetDialogItemText(item_handle(dialog_,i+3),text);
        size_t cap=i==2?sizeof editor_.form.name.s:sizeof editor_.form.lat.s;
        bool valid=text[0]<cap;
        for(unsigned j=1;j<=text[0];++j) if(text[j]<32 || text[j]>126) valid=false;
        if(!valid) {
            focus_=i; select(dialog_,i+3);
            message(i==2?"Use at most 47 plain characters for the name.":"Use at most 15 plain characters for a coordinate.");
            return false;
        }
        sv value(reinterpret_cast<char *>(text+1),text[0]);
        if(i==0) editor_.form.lat.set(value); else if(i==1) editor_.form.lon.set(value); else editor_.form.name.set(value);
    }
    editor_.form.metric=checked(dialog_,6); editor_.auto_refresh=checked(dialog_,7);
    return true;
}
bool LocationDialog::item(int item,SettingsStore &prefs,SettingsStore &legacy,Config &cfg,Model &model,long long now,Status *saved) {
    if(!dialog_) return false;
    if(item==2) { cancel(); *saved=okay(); return true; }
    if(item==6 || item==7) { check(dialog_,item,!checked(dialog_,item)); return true; }
    if(item>=3 && item<=5) { focus_=item-3; return true; }
    if(item!=1 || !read()) return false;
    *saved=editor_.apply(prefs,legacy,cfg,model,now);
    if(saved->ok()) { SetPort(prior_); DisposeDialog(dialog_); dialog_=nullptr; SetPort(prior_); }
    else {
        focus_=editor_.form.focus<3?editor_.form.focus:0;
        select(dialog_,focus_+3); message(saved->msg);
    }
    return true;
}
bool LocationDialog::event(EventRecord &e,SettingsStore &prefs,SettingsStore &legacy,Config &cfg,Model &model,long long now,Status *saved) {
    if(!dialog_) return false;
    if(e.what==keyDown || e.what==autoKey) {
        char ch=e.message & charCodeMask;
        if((e.modifiers & cmdKey) && (ch=='q' || ch=='Q')) return false;
        if(ch==27 || ((e.modifiers & cmdKey) && ch=='.')) { item(2,prefs,legacy,cfg,model,now,saved); return true; }
        if(ch==13 || ch==3) { item(1,prefs,legacy,cfg,model,now,saved); return true; }
        if(ch==9) {
            focus_=(focus_+((e.modifiers & shiftKey)?2:1))%3;
            select(dialog_,focus_+3); return true;
        }
    } else if(e.what==mouseDown) {
        WindowPtr w=nullptr;
        if(FindWindow(e.where,&w)==inMenuBar) return false;
        if(w!=window()) return true; // Application-modal content; no refresh behind the dialog.
    } else if(e.what!=activateEvt && e.what!=nullEvent) return false;
    DialogPtr selected=nullptr; short selected_item=0;
    bool hit; { DialogPort port(dialog_); hit=DialogSelect(&e,&selected,&selected_item); }
    if(hit && selected==dialog_) item(selected_item,prefs,legacy,cfg,model,now,saved);
    return true;
}
}
