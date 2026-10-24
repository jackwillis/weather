#include "cde.hpp"
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

int text_width(XFontStruct *f, sv s) { return XTextWidth(f, s.data(), static_cast<int>(s.size())); }

void text(Theme &t, Drawable w, XFontStruct *f, int x, int baseline, sv s, unsigned long color)
{
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
	size_t n = s.size() < cap - 1 ? s.size() : cap - 1;
	memcpy(out, s.data(), n);
	out[n] = 0;
	if (text_width(f, sv(out, n)) <= maxw) return n;
	while (n > 0) {
		n--;
		if (n + 3 < cap) { memcpy(out + n, "...", 3); }
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

void field(Theme &t, Drawable w, Rect r, const char *s, size_t n, size_t cur, bool focus)
{
	fill(t, w, r, t.field);
	shadow(t, w, r, 2, false);
	int base = r.y + (r.h + t.normal->ascent - t.normal->descent) / 2;
	text(t, w, t.normal, r.x + 6, base, sv(s, n), t.fg);
	if (focus) {
		int cx = r.x + 6 + text_width(t.normal, sv(s, cur));
		XSetForeground(t.dpy, t.gc, t.fg);
		XDrawLine(t.dpy, w, t.gc, cx, r.y + 4, cx, r.y + r.h - 5);
		XDrawRectangle(t.dpy, w, t.gc, r.x - 2, r.y - 2, static_cast<unsigned>(r.w + 3), static_cast<unsigned>(r.h + 3));
	}
}

// ---------------------------------------------------------------- weather art

static void disc(Theme &t, Drawable w, int cx, int cy, int r, unsigned long c)
{
	XSetForeground(t.dpy, t.gc, c);
	XFillArc(t.dpy, w, t.gc, cx - r, cy - r, static_cast<unsigned>(2 * r), static_cast<unsigned>(2 * r), 0, 360 * 64);
}

static void cloud(Theme &t, Drawable w, int cx, int cy, int s100, unsigned long body, unsigned long edge)
{
	auto sc = [&](int v) { return v * s100 / 100; };
	for (int pass = 0; pass < 2; pass++) {
		int grow = pass == 0 ? 2 : 0;
		unsigned long c = pass == 0 ? edge : body;
		disc(t, w, cx - sc(16), cy + sc(4), sc(12) + grow, c);
		disc(t, w, cx - sc(2), cy - sc(6), sc(16) + grow, c);
		disc(t, w, cx + sc(15), cy + sc(2), sc(13) + grow, c);
		XSetForeground(t.dpy, t.gc, c);
		XFillRectangle(t.dpy, w, t.gc, cx - sc(16), cy + sc(2) - grow, static_cast<unsigned>(sc(32)), static_cast<unsigned>(sc(14) + 2 * grow));
	}
	XSetForeground(t.dpy, t.gc, body);                     // hide the edge line inside the body
	XFillRectangle(t.dpy, w, t.gc, cx - sc(15), cy + sc(2), static_cast<unsigned>(sc(30)), static_cast<unsigned>(sc(13)));
}

static void sun(Theme &t, Drawable w, int cx, int cy, int r)
{
	unsigned long ray = t.rgb(255, 214, 64), core = t.rgb(255, 196, 32), rim = t.rgb(214, 150, 20);
	static const int dir[8][2] = {{100, 0}, {71, 71}, {0, 100}, {-71, 71}, {-100, 0}, {-71, -71}, {0, -100}, {71, -71}};
	XSetForeground(t.dpy, t.gc, ray);
	XSetLineAttributes(t.dpy, t.gc, 3, LineSolid, CapRound, JoinRound);
	for (const auto &d : dir)
		XDrawLine(t.dpy, w, t.gc, cx + d[0] * (r + 5) / 100, cy + d[1] * (r + 5) / 100, cx + d[0] * (r + 13) / 100, cy + d[1] * (r + 13) / 100);
	XSetLineAttributes(t.dpy, t.gc, 0, LineSolid, CapButt, JoinMiter);
	disc(t, w, cx, cy, r + 1, rim);
	disc(t, w, cx, cy, r, core);
}

static void moon(Theme &t, Drawable w, int cx, int cy, int r, unsigned long sky)
{
	disc(t, w, cx, cy, r, t.rgb(240, 236, 200));
	disc(t, w, cx + r / 2 + 2, cy - r / 3, r - 1, sky);
}

static void drops(Theme &t, Drawable w, int cx, int y, int n, int len, unsigned long c)
{
	XSetForeground(t.dpy, t.gc, c);
	XSetLineAttributes(t.dpy, t.gc, 2, LineSolid, CapRound, JoinRound);
	for (int i = 0; i < n; i++) {
		int x = cx - 18 + i * (36 / (n > 1 ? n - 1 : 1));
		int yy = y + (i % 2) * 6;
		XDrawLine(t.dpy, w, t.gc, x, yy, x - 4, yy + len);
	}
	XSetLineAttributes(t.dpy, t.gc, 0, LineSolid, CapButt, JoinMiter);
}

static void flakes(Theme &t, Drawable w, int cx, int y, unsigned long c)
{
	XSetForeground(t.dpy, t.gc, c);
	for (int i = 0; i < 5; i++) {
		int x = cx - 20 + i * 10, yy = y + (i % 2) * 9 + 4;
		XDrawLine(t.dpy, w, t.gc, x - 3, yy, x + 3, yy);
		XDrawLine(t.dpy, w, t.gc, x, yy - 3, x, yy + 3);
		XDrawLine(t.dpy, w, t.gc, x - 2, yy - 2, x + 2, yy + 2);
		XDrawLine(t.dpy, w, t.gc, x - 2, yy + 2, x + 2, yy - 2);
	}
}

void weather_icon(Theme &t, Drawable w, Rect a, long code, bool night)
{
	unsigned long sky = night ? t.rgb(38, 50, 86) : t.rgb(124, 172, 224);
	fill(t, w, a, sky);
	int cx = a.x + a.w / 2, cy = a.y + a.h / 2 - 4;
	unsigned long white = t.rgb(248, 249, 252), grey = t.rgb(160, 166, 184), dark = t.rgb(96, 102, 124);
	unsigned long edge = t.rgb(110, 124, 160), blue = night ? t.rgb(110, 160, 235) : t.rgb(48, 96, 190);

	bool snowy = (code >= 71 && code <= 77) || code == 85 || code == 86;
	bool rainy = (code >= 51 && code <= 67) || (code >= 80 && code <= 82);
	bool storm = code >= 95 && code <= 99;
	bool foggy = code == 45 || code == 48;

	if (code == 0 || code == 1) {
		if (night) { moon(t, w, cx, cy, 24, sky); }
		else sun(t, w, cx, cy, 22);
		if (night) {                                               // a few stars
			XSetForeground(t.dpy, t.gc, white);
			XFillRectangle(t.dpy, w, t.gc, a.x + 14, a.y + 14, 2, 2);
			XFillRectangle(t.dpy, w, t.gc, a.x + a.w - 22, a.y + 22, 2, 2);
			XFillRectangle(t.dpy, w, t.gc, a.x + a.w - 30, a.y + a.h - 24, 2, 2);
		}
	} else if (code == 2) {
		if (night) moon(t, w, cx - 14, cy - 14, 18, sky); else sun(t, w, cx - 14, cy - 14, 16);
		cloud(t, w, cx + 4, cy + 10, 100, white, edge);
	} else if (foggy) {
		XSetForeground(t.dpy, t.gc, grey);
		XSetLineAttributes(t.dpy, t.gc, 4, LineSolid, CapRound, JoinRound);
		for (int i = 0; i < 4; i++)
			XDrawLine(t.dpy, w, t.gc, cx - 28 + (i % 2) * 8, cy - 16 + i * 14, cx + 26 - ((i + 1) % 2) * 8, cy - 16 + i * 14);
		XSetLineAttributes(t.dpy, t.gc, 0, LineSolid, CapButt, JoinMiter);
	} else if (code == 3) {
		cloud(t, w, cx + 8, cy - 8, 80, grey, edge);
		cloud(t, w, cx - 4, cy + 8, 100, white, edge);
	} else if (storm) {
		cloud(t, w, cx, cy - 10, 100, dark, edge);
		XPoint bolt[6] = {{static_cast<short>(cx + 4), static_cast<short>(cy + 6)}, {static_cast<short>(cx - 8), static_cast<short>(cy + 26)},
		    {static_cast<short>(cx - 1), static_cast<short>(cy + 26)}, {static_cast<short>(cx - 6), static_cast<short>(cy + 42)},
		    {static_cast<short>(cx + 10), static_cast<short>(cy + 20)}, {static_cast<short>(cx + 3), static_cast<short>(cy + 20)}};
		XSetForeground(t.dpy, t.gc, t.rgb(255, 214, 64));
		XFillPolygon(t.dpy, w, t.gc, bolt, 6, Nonconvex, CoordModeOrigin);
		if (code >= 96) drops(t, w, cx - 12, cy + 12, 2, 8, blue);
	} else if (snowy) {
		cloud(t, w, cx, cy - 10, 100, white, edge);
		flakes(t, w, cx, cy + 14, white);
	} else if (rainy) {
		bool heavy = code == 55 || code == 65 || code == 67 || code == 82;
		cloud(t, w, cx, cy - 10, 100, grey, edge);
		drops(t, w, cx, cy + 14, heavy ? 6 : 4, heavy ? 14 : 10, blue);
	} else {
		cloud(t, w, cx, cy, 100, white, edge);                     // unknown: a plain cloud
	}
	shadow(t, w, a, 1, false);
}

} // namespace cde
