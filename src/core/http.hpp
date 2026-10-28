// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// HTTP/1.0 client side: build the request, parse the response. No chunked
// encoding is needed because we send HTTP/1.0 with Connection: close.
#pragma once
#include "status.hpp"
#include "text.hpp"

namespace wx {

// Opaque Last-Modified value: bounded printable ASCII, safe to echo as one header.
constexpr bool http_validator(sv value)
{
	if(value.size()>=64) return false;
	for(unsigned char c:value) if(c<32 || c>126) return false;
	return true;
}

// MET sends IMF-fixdate in UTC. Parse without libc/time_t (32-bit targets).
// Invalid/obsolete dates fail closed rather than authorizing an early request.
constexpr bool http_date(sv text,long long *out)
{
    if(text.size()!=29 || text[3]!=',' || text[4]!=' ' || text[7]!=' ' || text[11]!=' ' ||
       text[16]!=' ' || text[19]!=':' || text[22]!=':' || text.substr(25)!=" GMT") return false;
    int values[5]={};
    const size_t starts[]={5,12,17,20,23};
    const size_t widths[]={2,4,2,2,2};
    for(size_t i=0;i<5;i++) for(size_t j=0;j<widths[i];j++) {
        char c=text[starts[i]+j]; if(c<'0' || c>'9') return false;
        values[i]=values[i]*10+c-'0';
    }
    int day=values[0],year=values[1],hour=values[2],minute=values[3],second=values[4];
    if(year<1970 || hour>23 || minute>59 || second>59) return false;
    constexpr sv months="JanFebMarAprMayJunJulAugSepOctNovDec";
    int month=0;
    for(int i=0;i<12;i++) if(text.substr(8,3)==months.substr(3*i,3)) month=i+1;
    if(!month) return false;
    constexpr int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    bool leap=year%4==0 && (year%100!=0 || year%400==0);
    if(day<1 || day>lengths[month-1]+(month==2 && leap?1:0)) return false;
    long long y=year-1;
    long long days=365LL*(year-1970)+(y/4-y/100+y/400)-(1969/4-1969/100+1969/400);
    for(int i=1;i<month;i++) days+=lengths[i-1]+(i==2 && leap?1:0);
    days+=day-1;
    constexpr sv weekdays="SunMonTueWedThuFriSat";
    if(text.substr(0,3)!=weekdays.substr(static_cast<size_t>((days+4)%7)*3,3)) return false;
    *out=days*86400+hour*3600+minute*60+second;
    return true;
}

struct HttpResponse {
	Status st;
	int code = 0;
	sv body;
	sv last_modified = {};
	long long expires = 0;
};

constexpr HttpResponse http_parse(sv raw)
{
	if (raw.size() < 12 || raw.substr(0, 5) != sv("HTTP/")) {
		// Bytes that stop inside "HTTP/..." (or no bytes at all) are a connection that dropped: retryable, unlike garbage.
		size_t n = raw.size() < 5 ? raw.size() : 5;
		if (raw.size() < 12 && raw.substr(0, n) == sv("HTTP/").substr(0, n))
			return {make(Err::Truncated, "response cut off before the status line"), 0, {}};
		return {make(Err::Parse, "not an HTTP response"), 0, {}};
	}
	size_t eol = find(raw, "\r\n");
	if (eol == sv::npos) return {make(Err::Truncated, "response cut off in status line"), 0, {}};
	// "HTTP/1.x SSS reason"
	size_t sp = find(raw, " ");
	if (sp == sv::npos || sp + 4 > eol) return {make(Err::Parse, "bad status line"), 0, {}};
	int code = 0;
	for (size_t i = sp + 1; i < sp + 4; i++) {
		if (raw[i] < '0' || raw[i] > '9') return {make(Err::Parse, "bad status code"), 0, {}};
		code = code * 10 + (raw[i] - '0');
	}
	size_t hend = find(raw, "\r\n\r\n");
	if (hend == sv::npos) return {make(Err::Truncated, "response cut off in headers"), code, {}};

	size_t clen = 0; bool has_length=false;
	sv modified; bool have_modified=false, have_expires=false;
	long long expires=0;
	size_t pos = eol + 2;
	while (pos < hend + 2) {
		size_t le = find(raw, "\r\n", pos);
		sv line = raw.substr(pos, le - pos);
		size_t colon = find(line, ":");
		if (colon != sv::npos) {
			sv name = line.substr(0, colon);
			sv val = trim(line.substr(colon + 1));
			if(eq_nocase(name,"last-modified")) {
				if(have_modified || !http_validator(val)) return {make(Err::Parse,"invalid Last-Modified"),code,{}};
				modified=val; have_modified=true;
			} else if(eq_nocase(name,"expires")) {
                if(have_expires || !http_date(val,&expires)) return {make(Err::Parse,"invalid Expires"),code,{}};
                have_expires=true;
            } else if (eq_nocase(name, "content-length")) {
				if(val.empty()) return {make(Err::Parse,"bad Content-Length"),code,{}};
				size_t length=0;
				for(char c:val) {
					if(c<'0' || c>'9') return {make(Err::Parse,"bad Content-Length"),code,{}};
					size_t digit=static_cast<size_t>(c-'0');
					if(length>(static_cast<size_t>(-1)-digit)/10) return {make(Err::Parse,"Content-Length overflow"),code,{}};
					length=length*10+digit;
				}
				if(has_length && length!=clen) return {make(Err::Parse,"conflicting Content-Length"),code,{}};
				clen=length; has_length=true;
			} else if (eq_nocase(name, "transfer-encoding")) {
				// No transfer coding is implemented; never treat encoded bytes as JSON.
				return {make(Err::BadData, "transfer encoding not supported"), code, {}};
			}
		}
		pos = le + 2;
	}
	sv body = raw.substr(hend + 4);
	if(code==304) {
		if(!body.empty()) return {make(Err::BadData,"304 response contains a body"),code,{}};
		return {okay(),code,body,modified,expires};
	}
	if (has_length) {
		if (body.size() < clen)
			return {make(Err::Truncated, "body shorter than Content-Length"), code, body};
		body = body.substr(0,clen);
	}
	return {okay(), code, body, modified, expires};
}

// Write "GET path HTTP/1.0" into out. False if it does not fit.
template <size_t N>
constexpr bool http_request(Buf<N> &out, sv host, sv path, sv agent, sv modified = {})
{
	if(!http_validator(modified)) return false;
	out.add("GET ").add(path).add(" HTTP/1.0\r\nHost: ").add(host)
	    .add("\r\nUser-Agent: ").add(agent)
	    .add("\r\nAccept: application/json\r\n");
	if(!modified.empty()) out.add("If-Modified-Since: ").add(modified).add("\r\n");
	out.add("Connection: close\r\n\r\n");
	return !out.truncated;
}

} // namespace wx
