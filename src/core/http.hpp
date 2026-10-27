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

struct HttpResponse {
	Status st;
	int code = 0;
	sv body;
	sv last_modified = {};
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

	long clen = -1;
	sv modified; bool have_modified=false;
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
			} else if (eq_nocase(name, "content-length")) {
				Num n = parse_fixed(val, 0);
				if (!n.ok) return {make(Err::Parse, "bad Content-Length"), code, {}};
				clen = n.v;
			} else if (eq_nocase(name, "transfer-encoding") && find(val, "chunked") != sv::npos) {
				return {make(Err::BadData, "chunked encoding not supported"), code, {}};
			}
		}
		pos = le + 2;
	}
	sv body = raw.substr(hend + 4);
	if(code==304) {
		if(!body.empty()) return {make(Err::BadData,"304 response contains a body"),code,{}};
		return {okay(),code,body,modified};
	}
	if (clen >= 0) {
		if (static_cast<long>(body.size()) < clen)
			return {make(Err::Truncated, "body shorter than Content-Length"), code, body};
		body = body.substr(0, static_cast<size_t>(clen));
	}
	return {okay(), code, body, modified};
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
