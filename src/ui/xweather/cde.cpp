// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#include "cde.hpp"
#include "app/weather_art.hpp"
#include <cstdio>
#include <cstring>

namespace cde {

static bool res_color(Display *d, Colormap cm, const char *name, XColor *out)
{
	const char *v = XGetDefault(d, "xweather", name);
	if (!v) return false;
	XColor exact;
	if (!XParseColor(d, cm, v, &exact)) return false;
	*out = exact;
	return true;
}

static unsigned long scaled(Display *d, Colormap cm, XColor base, int num, int den, bool toward_white, unsigned long fb)
{
	XColor c = base;
	auto mix = [&](unsigned short v) -> unsigned short {
		long x = toward_white ? v + (65535 - v) * num / den : static_cast<long>(v) * num / den;
		return static_cast<unsigned short>(x > 65535 ? 65535 : x);
	};
	c.red = mix(base.red); c.green = mix(base.green); c.blue = mix(base.blue);
	c.flags = DoRed | DoGreen | DoBlue;
	return XAllocColor(d, cm, &c) ? c.pixel : fb;
}

static XFontStruct *load_font(Display *d, const char *resource, const char *const *patterns)
{
	if (const char *v = XGetDefault(d, "xweather", resource))
		if (XFontStruct *f = XLoadQueryFont(d, v)) return f;
	for (; *patterns; patterns++)
		if (XFontStruct *f = XLoadQueryFont(d, *patterns)) return f;
	return nullptr;
}

bool Theme::init(Display *d, int scr)
{
	dpy = d; screen = scr;
	cmap = DefaultColormap(d, scr);
	gc = XCreateGC(d, RootWindow(d, scr), 0, nullptr);
	fg = BlackPixel(d, scr);
	bg = WhitePixel(d, scr); top = WhitePixel(d, scr); bot = BlackPixel(d, scr);
	sel = bg; field = WhitePixel(d, scr); dim = fg;

	// CDE's blue-grey. The light and dark shadows and the selection colour are derived
	// from the background, the way CDE derives them (approximately: UNVERIFIED against real CDE).
	XColor base;
	base.red = 174 * 257; base.green = 178 * 257; base.blue = 195 * 257; base.flags = DoRed | DoGreen | DoBlue;
	res_color(d, cmap, "background", &base);
	XColor tmp = base;
	if (XAllocColor(d, cmap, &tmp)) bg = tmp.pixel;
	top = scaled(d, cmap, base, 55, 100, true, WhitePixel(d, scr));
	bot = scaled(d, cmap, base, 50, 100, false, BlackPixel(d, scr));
	sel = scaled(d, cmap, base, 82, 100, false, bot);
	field = scaled(d, cmap, base, 78, 100, true, WhitePixel(d, scr));
	dim = scaled(d, cmap, base, 38, 100, false, fg);
	XColor f;
	if (res_color(d, cmap, "foreground", &f) && XAllocColor(d, cmap, &f)) fg = f.pixel;

	static const char *const nf[] = {"-adobe-helvetica-medium-r-normal--12-*-75-75-*-*-iso8859-1",
	    "-*-helvetica-medium-r-normal--12-*-*-*-*-*-iso8859-1", "-*-*-medium-r-normal--12-*-*-*-p-*-iso8859-1", "fixed", nullptr};
	static const char *const bf[] = {"-adobe-helvetica-bold-r-normal--12-*-75-75-*-*-iso8859-1",
	    "-*-helvetica-bold-r-normal--12-*-*-*-*-*-iso8859-1", "-*-*-bold-r-normal--12-*-*-*-p-*-iso8859-1", "fixed", nullptr};
	static const char *const gf[] = {"-adobe-helvetica-bold-r-normal--24-*-75-75-*-*-iso8859-1",
	    "-adobe-helvetica-bold-r-normal--18-*-75-75-*-*-iso8859-1", "-*-helvetica-bold-r-normal--17-*-*-*-*-*-iso8859-1",
	    "-adobe-helvetica-bold-r-normal--14-*-100-100-*-*-iso8859-1", nullptr};
	normal = load_font(d, "font", nf);
	bold = load_font(d, "boldFont", bf);
	big = load_font(d, "bigFont", gf);
	if (!normal) { fprintf(stderr, "xweather: no usable font (not even 'fixed')\n"); return false; }
	if (!bold) bold = normal;
	if (!big) big = bold;
	return true;
}

unsigned long Theme::rgb(int r, int g, int b)
{
	for (int i = 0; i < ncache_; i++)
		if (cache_[i].r == r && cache_[i].g == g && cache_[i].b == b) return cache_[i].pixel;
	XColor c;
	c.red = static_cast<unsigned short>(r * 257); c.green = static_cast<unsigned short>(g * 257);
	c.blue = static_cast<unsigned short>(b * 257); c.flags = DoRed | DoGreen | DoBlue;
	unsigned long px = XAllocColor(dpy, cmap, &c) ? c.pixel : fg;
	if (ncache_ < 48) cache_[ncache_++] = Cached{r, g, b, px};
	return px;
}

void fill(Theme &t, Drawable w, Rect r, unsigned long color)
{
	XSetForeground(t.dpy, t.gc, color);
	XFillRectangle(t.dpy, w, t.gc, r.x, r.y, static_cast<unsigned>(r.w), static_cast<unsigned>(r.h));
}

void bitmap1(Theme &t, Drawable w, int x, int y, const unsigned char *bits, int bw, int bh, int rowbytes, unsigned long color)
{
	XSetForeground(t.dpy, t.gc, color);
	for (int row = 0; row < bh; row++) {
		const unsigned char *r = bits + static_cast<size_t>(row) * static_cast<size_t>(rowbytes);
		int run = -1;
		for (int col = 0; col <= bw; col++) {
			bool ink = col < bw && (r[col >> 3] & (0x80 >> (col & 7)));
			if (ink && run < 0) run = col;
			if (!ink && run >= 0) {
				XFillRectangle(t.dpy, w, t.gc, x + run, y + row, static_cast<unsigned>(col - run), 1);
				run = -1;
			}
		}
	}
}

void shadow(Theme &t, Drawable w, Rect r, int thick, bool out)
{
	unsigned long tl = out ? t.top : t.bot, br = out ? t.bot : t.top;
	for (int i = 0; i < thick; i++) {
		XSetForeground(t.dpy, t.gc, tl);
		XDrawLine(t.dpy, w, t.gc, r.x + i, r.y + i, r.x + r.w - 1 - i, r.y + i);
		XDrawLine(t.dpy, w, t.gc, r.x + i, r.y + i, r.x + i, r.y + r.h - 1 - i);
		XSetForeground(t.dpy, t.gc, br);
		XDrawLine(t.dpy, w, t.gc, r.x + i, r.y + r.h - 1 - i, r.x + r.w - 1 - i, r.y + r.h - 1 - i);
		XDrawLine(t.dpy, w, t.gc, r.x + r.w - 1 - i, r.y + i, r.x + r.w - 1 - i, r.y + r.h - 1 - i);
	}
}

int text_width(XFontStruct *f, sv s) { return s.empty()?0:XTextWidth(f, s.data(), static_cast<int>(s.size())); }

void text(Theme &t, Drawable w, XFontStruct *f, int x, int baseline, sv s, unsigned long color)
{
	if(s.empty()) return;
	XSetFont(t.dpy, t.gc, f->fid);
	XSetForeground(t.dpy, t.gc, color);
	XDrawString(t.dpy, w, t.gc, x, baseline, s.data(), static_cast<int>(s.size()));
}

void text_center(Theme &t, Drawable w, XFontStruct *f, Rect r, sv s, unsigned long color)
{
	int tw = text_width(f, s);
	int base = r.y + (r.h + f->ascent - f->descent) / 2;
	text(t, w, f, r.x + (r.w - tw) / 2, base, s, color);
}

size_t fit_text(XFontStruct *f, sv s, int maxw, char *out, size_t cap)
{
	if(!cap) return 0;
	size_t n = s.size() < cap - 1 ? s.size() : cap - 1;
	if(n) memcpy(out, s.data(), n);
	out[n] = 0;
	if (text_width(f, sv(out, n)) <= maxw) return n;
	while (n > 0) {
		n--;
		if (n + 3 >= cap) continue;
		memcpy(out + n, "...", 3);
		if (text_width(f, sv(out, n + 3)) <= maxw) { out[n + 3] = 0; return n + 3; }
	}
	out[0] = 0;
	return 0;
}

void button(Theme &t, Drawable w, Rect r, sv label, bool pressed, bool is_default)
{
	if (is_default) {                        // CDE's default button: an extra inset ring around it
		Rect ring = Rect{r.x - 4, r.y - 4, r.w + 8, r.h + 8};
		shadow(t, w, ring, 2, false);
	}
	fill(t, w, r, pressed ? t.sel : t.bg);
	shadow(t, w, r, 2, !pressed);
	Rect lab = r;
	if (pressed) { lab.x += 1; lab.y += 1; }
	text_center(t, w, t.bold, lab, label, t.fg);
}

void toggle(Theme &t, Drawable w, Rect r, sv label, bool on, bool radio)
{
	Rect ind{r.x + 2, r.y + (r.h - 13) / 2, 13, 13};
	if (radio) {
		int cx = ind.x + 6, cy = ind.y + 6;
		XPoint d[5] = {{static_cast<short>(cx), static_cast<short>(cy - 7)}, {static_cast<short>(cx + 7), static_cast<short>(cy)},
		    {static_cast<short>(cx), static_cast<short>(cy + 7)}, {static_cast<short>(cx - 7), static_cast<short>(cy)},
		    {static_cast<short>(cx), static_cast<short>(cy - 7)}};
		XSetForeground(t.dpy, t.gc, on ? t.sel : t.bg);
		XFillPolygon(t.dpy, w, t.gc, d, 4, Convex, CoordModeOrigin);
		XSetForeground(t.dpy, t.gc, on ? t.bot : t.top);
		XDrawLines(t.dpy, w, t.gc, d, 3, CoordModeOrigin);
		XSetForeground(t.dpy, t.gc, on ? t.top : t.bot);
		XDrawLine(t.dpy, w, t.gc, d[2].x, d[2].y, d[3].x, d[3].y);
		XDrawLine(t.dpy, w, t.gc, d[2].x, d[2].y, d[1].x, d[1].y);
		if (on) { XSetForeground(t.dpy, t.gc, t.fg); XFillRectangle(t.dpy, w, t.gc, cx - 1, cy - 1, 3, 3); }
	} else {
		fill(t, w, ind, on ? t.sel : t.bg);
		shadow(t, w, ind, 2, !on);
		if (on) {                            // a check mark
			XSetForeground(t.dpy, t.gc, t.fg);
			XDrawLine(t.dpy, w, t.gc, ind.x + 3, ind.y + 6, ind.x + 5, ind.y + 9);
			XDrawLine(t.dpy, w, t.gc, ind.x + 5, ind.y + 9, ind.x + 10, ind.y + 3);
			XDrawLine(t.dpy, w, t.gc, ind.x + 3, ind.y + 7, ind.x + 5, ind.y + 10);
			XDrawLine(t.dpy, w, t.gc, ind.x + 5, ind.y + 10, ind.x + 10, ind.y + 4);
		}
	}
	text(t, w, t.normal, r.x + 24, r.y + (r.h + t.normal->ascent - t.normal->descent) / 2, label, t.fg);
}

size_t field_start(XFontStruct *font, sv value, size_t cursor, int width)
{
	if (cursor > value.size()) cursor = value.size();
	size_t first = 0;
	while (first < cursor && text_width(font, value.substr(first, cursor - first)) >= width) first++;
	return first;
}

size_t field_position(XFontStruct *font, sv value, size_t cursor, int width, int x)
{
	size_t first = field_start(font, value, cursor, width);
	if (width <= 0) return first;
	if (x < 0) x = 0;
	if (x >= width) x = width - 1;
	size_t pos = first;
	while (pos < value.size() && text_width(font, value.substr(first, pos + 1 - first)) -
	    text_width(font, value.substr(pos, 1)) / 2 < x) pos++;
	return pos;
}

void field(Theme &t, Drawable w, Rect r, const char *s, size_t n, size_t cur, bool focus)
{
	if (r.w <= 6 || r.h <= 6) return;
	if (cur > n) cur = n;
	fill(t, w, r, t.field);
	shadow(t, w, r, 2, false);
	int base = r.y + (r.h + t.normal->ascent - t.normal->descent) / 2;
	sv value(s, n);
	size_t first = field_start(t.normal, value, cur, r.w - 12);
	XRectangle clip{static_cast<short>(r.x + 3), static_cast<short>(r.y + 3),
	    static_cast<unsigned short>(r.w - 6), static_cast<unsigned short>(r.h - 6)};
	XSetClipRectangles(t.dpy, t.gc, 0, 0, &clip, 1, Unsorted);
	text(t, w, t.normal, r.x + 6, base, value.substr(first), t.fg);
	if (focus) {
		int cx = r.x + 6 + text_width(t.normal, value.substr(first, cur - first));
		XSetForeground(t.dpy, t.gc, t.fg);
		XDrawLine(t.dpy, w, t.gc, cx, r.y + 4, cx, r.y + r.h - 5);
	}
	XSetClipMask(t.dpy, t.gc, None);
	if (focus) XDrawRectangle(t.dpy, w, t.gc, r.x - 2, r.y - 2, static_cast<unsigned>(r.w + 3), static_cast<unsigned>(r.h + 3));
}

// ---------------------------------------------------------------- weather art
// The geometry lives in app/weather_art.hpp (shared with the Mac OS 9 UI); this is the Xlib canvas it draws on.

namespace {
struct XCanvas {
	Theme &t;
	Drawable w;
	void color(wx::Rgb c) { XSetForeground(t.dpy, t.gc, t.rgb(c.r, c.g, c.b)); }
	void rect(int x, int y, int rw, int rh, wx::Rgb c)
	{
		if (rw <= 0 || rh <= 0) return;
		color(c);
		XFillRectangle(t.dpy, w, t.gc, x, y, static_cast<unsigned>(rw), static_cast<unsigned>(rh));
	}
	void disc(int cx, int cy, int r, wx::Rgb c)
	{
		color(c);
		XFillArc(t.dpy, w, t.gc, cx - r, cy - r, static_cast<unsigned>(2 * r), static_cast<unsigned>(2 * r), 0, 360 * 64);
	}
	void line(int x0, int y0, int x1, int y1, int width, wx::Rgb c)
	{
		color(c);
		XSetLineAttributes(t.dpy, t.gc, static_cast<unsigned>(width > 1 ? width : 0), LineSolid, width > 1 ? CapRound : CapButt, JoinRound);
		XDrawLine(t.dpy, w, t.gc, x0, y0, x1, y1);
		XSetLineAttributes(t.dpy, t.gc, 0, LineSolid, CapButt, JoinMiter);
	}
	void poly(const wx::ArtPt *p, int n, wx::Rgb c)
	{
		XPoint pts[16];
		if (n > 16) n = 16;
		for (int i = 0; i < n; i++) { pts[i].x = static_cast<short>(p[i].x); pts[i].y = static_cast<short>(p[i].y); }
		color(c);
		XFillPolygon(t.dpy, w, t.gc, pts, n, Nonconvex, CoordModeOrigin);
	}
};
}

void weather_icon(Theme &t, Drawable w, Rect a, long code, bool night, bool faded)
{
	XCanvas canvas{t, w};
	if (faded) {   // old data: fade toward the CDE panel grey, as the text dims (#62)
		wx::art::DimCanvas<XCanvas> dim{canvas, wx::Rgb{174, 178, 195}, 55};
		wx::art::picture(dim, wx::ArtRect{a.x, a.y, a.w, a.h}, code, night);
	} else wx::art::picture(canvas, wx::ArtRect{a.x, a.y, a.w, a.h}, code, night);
	shadow(t, w, a, 1, false);
}

} // namespace cde
