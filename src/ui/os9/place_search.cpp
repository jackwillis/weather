#include "place_search.hpp"
#include <Events.h>
#include <cstring>
namespace wx {
namespace {
struct Port {
    GrafPtr old;
    explicit Port(DialogPtr dialog) { GetPort(&old); SetPort(reinterpret_cast<WindowPtr>(dialog)); }
    ~Port() { SetPort(old); }
};
Handle handle(DialogPtr d,int item) {
    short type; Handle h=nullptr; Rect r; GetDialogItem(d,item,&type,&h,&r); return h;
}
void pascal_string(sv value,unsigned char *out) {
    size_t n=value.size()<255?value.size():255; out[0]=static_cast<unsigned char>(n);
    if(n) std::memcpy(out+1,value.data(),n);
}
void text(DialogPtr d,int item,sv value) {
    unsigned char p[256]; pascal_string(value,p); SetDialogItemText(handle(d,item),p);
}
void enable(DialogPtr d,int item,bool on) {
    HiliteControl(reinterpret_cast<ControlHandle>(handle(d,item)),on?0:255);
}
void title(DialogPtr d,int item,sv value) {
    unsigned char p[256]; pascal_string(value,p); SetControlTitle(reinterpret_cast<ControlHandle>(handle(d,item)),p);
}
}
Status PlaceDialog::open(WindowPtr parent) {
    if(visible()) return okay();
    picker_=PlacePicker{}; requested_=chosen_=false; query_[0]=0;
    if(!Get1Resource('DLOG',201) || !Get1Resource('DITL',201)) return make(Err::Io,"Search dialog resources are missing");
    GetPort(&prior_); parent_=parent;
    dialog_=GetNewDialog(201,nullptr,reinterpret_cast<WindowPtr>(-1L));
    if(!dialog_) { SetPort(prior_); return make(Err::Io,"Cannot open place search"); }
    { Port port(dialog_); SelectDialogItemText(dialog_,3,0,32767); }
    ShowWindow(window()); SelectWindow(window()); render(); SetPort(prior_); return okay();
}
void PlaceDialog::close() {
    if(!dialog_) return;
    SetPort(prior_); DisposeDialog(dialog_); dialog_=nullptr;
    if(parent_) SelectWindow(parent_);
    SetPort(prior_);
}
void PlaceDialog::cancel() {
    picker_.cancel(); requested_=chosen_=false;
    if(!picker_.busy) close();
}
void PlaceDialog::set_query(sv value) {
    if(!visible() || busy()) return;
    Port port(dialog_); text(dialog_,3,value); sync_query();
}
void PlaceDialog::sync_query() {
    unsigned char p[256]; GetDialogItemText(handle(dialog_,3),p);
    bool valid=p[0]<sizeof query_;
    for(unsigned i=1;i<=p[0];++i) if(p[i]<32 || p[i]>126) valid=false;
    if(!valid) {
        query_[0]=0; picker_.edited(); picker_.search.error=make(Err::Usage,"Use 2 to 63 plain ASCII characters"); render(); return;
    }
    sv next(reinterpret_cast<char *>(p+1),p[0]);
    if(next!=sv(query_)) {
        picker_.edited(); if(p[0]) std::memcpy(query_,p+1,p[0]); query_[p[0]]=0; render();
    }
}
void PlaceDialog::render() {
    if(!visible()) return;
    Port port(dialog_);
    for(size_t i=0;i<kPlaceLimit;++i) {
        Buf<160> label;
        if(i<picker_.search.results.count) {
            const Place &p=picker_.search.results.items[i];
            label.add(p.name); if(p.region[0]) label.add(", ").add(p.region);
            if(p.country[0]) label.add(" (").add(p.country).add(")");
        }
        // Fit control titles in the native font; details remain available below.
        while(label.n && TextWidth(label.s,0,static_cast<short>(label.n))>406) label.s[--label.n]=0;
        title(dialog_,static_cast<int>(i)+5,label.view());
        SetControlValue(reinterpret_cast<ControlHandle>(handle(dialog_,static_cast<int>(i)+5)),picker_.selected==static_cast<int>(i));
        enable(dialog_,static_cast<int>(i)+5,!busy() && i<picker_.search.results.count);
    }
    enable(dialog_,1,!busy() && picker_.selected>=0); enable(dialog_,4,!busy());
    Buf<192> detail;
    if(picker_.selected>=0) {
        const Place &p=picker_.search.results.items[picker_.selected];
        detail.add(p.name); if(p.region[0]) detail.add(", ").add(p.region);
        detail.add("  ").add(p.country).add("  ").add_fixed(p.lat_e4,4).add(", ").add_fixed(p.lon_e4,4);
        if(p.simplified) detail.add("  (ASCII name simplified)");
    }
    text(dialog_,12,detail.view());
    const char *status=busy()?"Searching... Escape cancels.":!picker_.search.error.ok()?picker_.search.error.msg:
        picker_.search.results.count?"Select a result, then Use. Apply in Location saves it.":"Type a place, then Search. No result is selected automatically.";
    text(dialog_,10,status); draw();
}
void PlaceDialog::draw() {
    if(!visible()) return;
    Port port(dialog_); DrawDialog(dialog_);
}
Status PlaceDialog::begin() {
    if(!visible()) return make(Err::Usage,"Search is closed");
    Port port(dialog_); sync_query(); Status s=picker_.begin(query_); render(); return s;
}
void PlaceDialog::finish(Status status,const PlaceResults &results) {
    picker_.finish(status,results);
    if(cancelled()) close(); else render();
}
bool PlaceDialog::take_choice(LocationForm &form) {
    bool result=chosen_ && picker_.choose(form).ok(); chosen_=false; return result;
}
void PlaceDialog::item(int item) {
    if(!visible()) return;
    if(item==2) { cancel(); return; }
    if(busy() || requested_) return;
    if(item==4) { picker_.edited(); requested_=true; render(); return; }
    if(item>=5 && item<=9) { picker_.select(static_cast<size_t>(item-5)); render(); return; }
    if(item==1) {
        LocationForm form;
        if(picker_.choose(form).ok()) { chosen_=true; close(); }
    }
}
bool PlaceDialog::event(EventRecord &e) {
    if(!visible()) return false;
    if(e.what==keyDown || e.what==autoKey) {
        char ch=e.message & charCodeMask;
        if((e.modifiers & cmdKey) && (ch=='q' || ch=='Q')) return false;
        if(ch==27 || ((e.modifiers & cmdKey) && ch=='.')) { cancel(); return true; }
        if(busy() || requested_) return true;
        if(ch==13 || ch==3) { item(picker_.selected>=0?1:4); return true; }
        if(ch==30 || ch==31) {
            int next=picker_.selected+(ch==30?-1:1);
            if(next<0) next=static_cast<int>(picker_.search.results.count)-1;
            if(next>=static_cast<int>(picker_.search.results.count)) next=0;
            if(next>=0) item(next+5);
            return true;
        }
        if(ch==9) { Port port(dialog_); SelectDialogItemText(dialog_,3,0,32767); return true; }
    } else if(e.what==mouseDown) {
        WindowPtr w=nullptr; if(FindWindow(e.where,&w)==inMenuBar) return false;
        if(w!=window()) return true;
    } else if(e.what!=activateEvt && e.what!=nullEvent) return false;
    DialogPtr selected=nullptr; short selected_item=0;
    bool hit; { Port port(dialog_); hit=DialogSelect(&e,&selected,&selected_item); if(!busy()) sync_query(); }
    if(hit && selected==dialog_) item(selected_item);
    return true;
}
}
