// Editing ~/.weatherrc without damaging it: change the keys we own, keep every other
// line, comment and blank exactly as the user left them. Pure text in, pure text out;
// reading and writing the bytes is the platform's SettingsStore.
#pragma once
#include "config.hpp"

namespace wx {

struct KeyVal {
	sv key;
	sv val;
};

// Where settings live. Per OS: a dotfile on Unix, the Preferences folder on OS 9.
struct SettingsStore {
	void *ctx = nullptr;
	// Missing storage is not an error: load returns n = 0.
	Status (*load)(SettingsStore &, char *buf, size_t cap, size_t *n) = nullptr;
	// Must be atomic: after a crash the old or the new text is there, never half.
	Status (*save)(SettingsStore &, const char *text, size_t n) = nullptr;
};

namespace settings_detail {
struct Out {
	char *p;
	size_t cap, n;
	bool over;
	void put(char c) { if (n + 1 < cap) p[n++] = c; else over = true; }
	void put(sv s) { for (char c : s) put(c); }
};
} // namespace settings_detail

// Rewrite `text` so that every key in kvs has the given value. All existing lines for a
// key are updated (the parser lets the last one win); keys that are absent are appended.
// An inline "# comment" after a replaced value is kept. The result must still parse.
inline Status settings_update(sv text, const KeyVal *kvs, size_t nkv, char *out, size_t cap, size_t *outn)
{
	for (size_t i = 0; i < nkv; i++) {
		sv v = kvs[i].val;
		if (kvs[i].key.empty() || find(kvs[i].key, "=") != sv::npos || find(kvs[i].key, "#") != sv::npos)
			return make(Err::Config, "bad settings key");
		for (char c : v)
			if (c == '\n' || c == '\r' || c == '#') return make(Err::Config, "settings value may not contain newline or '#'");
		if (trim(v).size() != v.size()) return make(Err::Config, "settings value may not start or end with space");
	}
	bool done[16] = {};
	if (nkv > 16) return make(Err::Config, "too many settings at once");

	settings_detail::Out o{out, cap, 0, false};
	// New keys end with the file's own line ending: a lone CR if its first break is one (classic Mac), else LF.
	size_t first = line_break(text, 0);
	sv eol = (first < text.size() && text[first] == '\r' && (first + 1 >= text.size() || text[first + 1] != '\n')) ? sv("\r") : sv("\n");
	size_t pos = 0;
	while (pos < text.size()) {
		size_t end = after_break(text, line_break(text, pos));   // line including its terminator
		sv line = text.substr(pos, end - pos);
		sv body = line, tail;                                     // tail = line ending
		size_t t = body.size();
		while (t > 0 && (body[t - 1] == '\n' || body[t - 1] == '\r')) t--;
		tail = body.substr(t);
		body = body.substr(0, t);
		size_t hash = find(body, "#");
		sv content = hash == sv::npos ? body : body.substr(0, hash);
		sv comment = hash == sv::npos ? sv() : body.substr(hash);
		size_t eq = find(content, "=");
		bool replaced = false;
		if (eq != sv::npos) {
			sv k = trim(content.substr(0, eq));
			for (size_t i = 0; i < nkv; i++) {
				if (k == kvs[i].key) {
					o.put(kvs[i].key); o.put(" = "); o.put(kvs[i].val);
					if (!comment.empty()) { o.put("  "); o.put(comment); }
					o.put(tail);
					done[i] = true;
					replaced = true;
					break;
				}
			}
		}
		if (!replaced) o.put(line);
		pos = end;
	}
	for (size_t i = 0; i < nkv; i++) {
		if (done[i]) continue;
		if (o.n > 0 && out[o.n - 1] != '\n' && out[o.n - 1] != '\r') o.put(eol);
		o.put(kvs[i].key); o.put(" = "); o.put(kvs[i].val); o.put(eol);
	}
	if (o.over) return make(Err::Config, "settings file too large");
	out[o.n] = 0;
	*outn = o.n;
	Config probe;                                            // never write something we cannot read back
	Status s = config_parse(sv(out, o.n), &probe);
	if (!s.ok()) return make(Err::Config, s.msg);   // (a line number in text we generated would only confuse)
	*outn = o.n;
	return okay();
}

// Persist some keys: load, edit, save (atomically, via the store).
inline Status settings_save_kv(SettingsStore &st, const KeyVal *kv, size_t nkv)
{
	static char in[4096], out[4096];                         // single-threaded app; keeps stack small
	size_t n = 0;
	Status s = st.load(st, in, sizeof in, &n);
	if (!s.ok()) return s;
	size_t on = 0;
	s = settings_update(sv(in, n), kv, nkv, out, sizeof out, &on);
	if (!s.ok()) return s;
	return st.save(st, out, on);
}

// Persist location, name and units.
inline Status settings_save(SettingsStore &st, const Config &c)
{
	Buf<16> lat, lon;
	lat.add_fixed(c.lat_e4, 4);
	lon.add_fixed(c.lon_e4, 4);
	KeyVal kv[] = {
		{"latitude", lat.view()}, {"longitude", lon.view()},
		{"name", c.name}, {"units", c.metric ? "metric" : "imperial"},
	};
	return settings_save_kv(st, kv, 4);
}

} // namespace wx
