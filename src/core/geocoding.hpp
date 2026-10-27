// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Explicit place-name lookup. Fixed endpoint, bounded results, owned ASCII labels.
#pragma once
#include "json.hpp"
#include "fetch.hpp"

namespace wx {
constexpr size_t kPlaceLimit=5, kGeocodingBodyLimit=32768;
struct Place {
    long id=0, lat_e4=0, lon_e4=0;
    char name[48]={}, region[48]={}, country[3]={};
    bool simplified=false;
};
struct PlaceResults { Place items[kPlaceLimit]; size_t count=0; };

inline Status geocoding_path(sv query,char *out,size_t cap)
{
    query=trim(query);
    if(query.size()<2 || query.size()>63) return make(Err::Usage,"Search needs 2 to 63 ASCII characters");
    Buf<256> path; path.add("/v1/search?name=");
    constexpr char hex[]="0123456789ABCDEF";
    for(unsigned char c:query) {
        if(c<32 || c>126) return make(Err::Usage,"Search accepts printable ASCII only");
        if((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='_' || c=='.' || c=='~') path.put(c);
        else { path.put('%'); path.put(hex[c>>4]); path.put(hex[c&15]); }
    }
    path.add("&count=5&language=en&format=json");
    if(path.truncated || path.n>=cap) return make(Err::Usage,"Search request is too long");
    for(size_t i=0;i<=path.n;i++) out[i]=path.s[i];
    return okay();
}
namespace geocoding_detail {
inline int hex(char c) { return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:c>='A' && c<='F'?c-'A'+10:-1; }
inline bool hex4(sv s,size_t &i,unsigned &value)
{
    value=0;
    for(int k=0;k<4;k++) { if(i>=s.size()) return false; int h=hex(s[i++]); if(h<0) return false; value=value*16+static_cast<unsigned>(h); }
    return true;
}
// Decode Unicode scalars, rejecting overlong UTF-8, surrogates and controls.
// Classic TextEdit/X core fonts use ASCII here; one '?' per non-ASCII scalar.
inline bool string(sv s,size_t &i,char *out,size_t cap,bool *simplified)
{
    if(i>=s.size() || s[i++]!='"') return false;
    size_t n=0;
    while(i<s.size()) {
        unsigned c=static_cast<unsigned char>(s[i++]);
        if(c=='"') { if(out) out[n]=0; return true; }
        if(c=='\\') {
            if(i>=s.size()) return false;
            char e=s[i++];
            if(e=='u') {
                if(!hex4(s,i,c)) return false;
                if(c>=0xd800 && c<=0xdbff) {
                    if(i+2>s.size() || s[i]!='\\' || s[i+1]!='u') return false;
                    i+=2; unsigned low; if(!hex4(s,i,low) || low<0xdc00 || low>0xdfff) return false;
                    c=0x10000+(c-0xd800)*1024+low-0xdc00;
                } else if(c>=0xdc00 && c<=0xdfff) return false;
            } else if(e=='"' || e=='\\' || e=='/') c=static_cast<unsigned char>(e);
            else if(e=='b' || e=='f' || e=='n' || e=='r' || e=='t') { if(out) return false; c=' '; }
            else return false;
        } else if(c>=128) {
            unsigned follow=0,min=0;
            if(c>=0xc2 && c<=0xdf) { follow=1; min=0x80; c&=31; }
            else if(c>=0xe0 && c<=0xef) { follow=2; min=0x800; c&=15; }
            else if(c>=0xf0 && c<=0xf4) { follow=3; min=0x10000; c&=7; }
            else return false;
            while(follow--) { if(i>=s.size()) return false; unsigned b=static_cast<unsigned char>(s[i++]); if((b&0xc0)!=0x80) return false; c=c*64+(b&63); }
            if(c<min || c>0x10ffff || (c>=0xd800 && c<=0xdfff)) return false;
        } else if(c<32) return false;
        if(out) {
            if(c<32 || c==127) return false;
            if(n+1>=cap) return false;
            if(c>126 || c=='#') { out[n++]='?'; *simplified=true; }
            else out[n++]=static_cast<char>(c);
        }
    }
    return false;
}
// Validate every token, including unused fields, before using JSON navigation.
inline bool value(sv s,size_t &i,size_t depth=0)
{
    if(depth>kJsonMaxDepth) return false;
    i=json_ws(s,i); if(i>=s.size()) return false;
    char c=s[i];
    if(c=='"') return string(s,i,nullptr,0,nullptr);
    if(c=='[' || c=='{') {
        ++i; i=json_ws(s,i); char close=c=='['?']':'}';
        if(i<s.size() && s[i]==close) { ++i; return true; }
        for(;;) {
            if(c=='{') { if(!string(s,i,nullptr,0,nullptr)) return false; i=json_ws(s,i); if(i>=s.size() || s[i++]!=':') return false; }
            if(!value(s,i,depth+1)) return false;
            i=json_ws(s,i); if(i>=s.size()) return false;
            if(s[i]==close) { ++i; return true; }
            if(s[i++]!=',') return false;
            i=json_ws(s,i);
        }
    }
    for(sv literal:{sv("true"),sv("false"),sv("null")}) if(s.substr(i,literal.size())==literal) { i+=literal.size(); return true; }
    if(s[i]=='-') ++i;
    if(i>=s.size()) return false;
    if(s[i]=='0') ++i;
    else { if(s[i]<'1' || s[i]>'9') return false; while(i<s.size() && s[i]>='0' && s[i]<='9') ++i; }
    if(i<s.size() && s[i]=='.') { size_t start=++i; while(i<s.size() && s[i]>='0' && s[i]<='9') ++i; if(i==start) return false; }
    if(i<s.size() && (s[i]=='e' || s[i]=='E')) { ++i; if(i<s.size() && (s[i]=='+' || s[i]=='-')) ++i; size_t start=i; while(i<s.size() && s[i]>='0' && s[i]<='9') ++i; if(i==start) return false; }
    return true;
}
inline bool label(sv obj,sv key,char *out,size_t cap,bool &simplified,bool required)
{
    JsonLookup field=json_get(obj,key);
    if(field.r==JsonFind::Absent) return !required;
    if(field.r!=JsonFind::Found) return false;
    size_t pos=0;
    if(!string(field.value,pos,out,cap,&simplified) || pos!=field.value.size()) return false;
    return !required || out[0];
}
}
inline Status geocoding_parse(sv body,PlaceResults *out)
{
    if(body.size()>kGeocodingBodyLimit) return make(Err::BadData,"Place response is too large");
    size_t end=0;
    if(!geocoding_detail::value(body,end) || json_ws(body,end)!=body.size()) return make(Err::Parse,"Malformed place response");
    JsonLookup error=json_get(body,"error"), list=json_get(body,"results");
    if(error.r==JsonFind::Malformed || (error.r==JsonFind::Found && error.value!="false")) return make(Err::BadData,"Place service returned an error");
    PlaceResults next;
    if(list.r==JsonFind::Absent) { *out=next; return okay(); }
    if(list.r!=JsonFind::Found || list.value.empty() || list.value[0]!='[') return make(Err::Parse,"Place list is missing");
    sv array=list.value; size_t pos=json_ws(array,1);
    while(pos<array.size() && array[pos]!=']') {
        if(next.count==kPlaceLimit) return make(Err::BadData,"Too many place results");
        size_t stop=json_skip(array,pos); if(stop==sv::npos) return make(Err::Parse,"Malformed place result");
        sv obj=array.substr(pos,stop-pos); Place &p=next.items[next.count];
        Num id=json_num(json_get(obj,"id").value,0), lat=json_num(json_get(obj,"latitude").value,4), lon=json_num(json_get(obj,"longitude").value,4);
        bool integer_id=!json_get(obj,"id").value.empty();
        for(char c:json_get(obj,"id").value) if(c<'0' || c>'9') integer_id=false;
        if(!integer_id || !id.ok || id.v<=0 || !lat.ok || lat.v < -900000 || lat.v>900000 || !lon.ok || lon.v < -1800000 || lon.v>1800000) return make(Err::BadData,"Place has invalid coordinates or identity");
        p.id=id.v; p.lat_e4=lat.v; p.lon_e4=lon.v;
        if(!geocoding_detail::label(obj,"name",p.name,sizeof p.name,p.simplified,true) || !geocoding_detail::label(obj,"admin1",p.region,sizeof p.region,p.simplified,false) || !geocoding_detail::label(obj,"country_code",p.country,sizeof p.country,p.simplified,false)) return make(Err::BadData,"Place label cannot be displayed");
        if(p.country[0] && (!(p.country[0]>='A' && p.country[0]<='Z') || !(p.country[1]>='A' && p.country[1]<='Z'))) return make(Err::BadData,"Invalid place country code");
        ++next.count; pos=json_ws(array,stop); if(pos<array.size() && array[pos]==',') pos=json_ws(array,pos+1);
    }
    *out=next; return okay();
}
// Stable tab-separated listing: ordinal, name, region/country, coordinates.
template<size_t N> inline void format_places(Buf<N> &out,const PlaceResults &places)
{
    out.add("Places from Open-Meteo / GeoNames\n");
    if(!places.count) out.add("No matching places.\n");
    for(size_t i=0;i<places.count && i<kPlaceLimit;i++) {
        const Place &p=places.items[i];
        out.add_int(static_cast<long>(i+1)).add('\t').add(p.name).add('\t').add(p.region);
        if(p.country[0]) { if(p.region[0]) out.add(' '); out.add(p.country); }
        out.add('\t').add_fixed(p.lat_e4,4).add(',').add_fixed(p.lon_e4,4);
        if(p.simplified) out.add(" (ASCII simplified)");
        out.add('\n');
    }
    if(places.count) out.add("Choose coordinates with -l lat,lon; use -n name and -S to save separately.\n");
}
inline Status search_places(Net &net,sv query,const FetchOpts &opts,char *buf,size_t cap,PlaceResults *out)
{
    char path[256]; Status s=geocoding_path(query,path,sizeof path); if(!s.ok()) return s;
    if(net.yield && !net.yield(net)) return make(Err::Cancelled,"Place search cancelled");
    FetchOpts bounded=opts; bounded.contact=nullptr;
    if(bounded.attempts<1 || bounded.attempts>3 || bounded.timeout_s<1 || bounded.timeout_s>60) return make(Err::Usage,"Invalid place search limits");
    bounded.backoff_ms=500;
    sv body;
    s=fetch_retry(net,"geocoding-api.open-meteo.com","443",true,path,bounded,buf,cap,&body);
    if(s.ok()) s=geocoding_parse(body,out);
    return s;
}
} // namespace wx
