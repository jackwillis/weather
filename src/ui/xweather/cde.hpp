// CDE-look drawing primitives on plain Xlib (immediate mode: callers redraw everything
// into a Pixmap, then copy it to the window). Not the Motif API: no Xt, no libXm.
// Resources (xrdb): xweather.background, .foreground, .font, .boldFont, .bigFont.
#pragma once
#include "core/text.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

// Xlib defines `Status` as a macro for int; our wx::Status must win.
#undef Status

namespace cde {

using wx::sv;

struct Rect {
	int x, y, w, h;
	bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
	Rect inset(int d) const { return Rect{x + d, y + d, w - 2 * d, h - 2 * d}; }
};

struct Theme {
	Display *dpy = nullptr;
	int screen = 0;
	Colormap cmap = 0;
	GC gc = nullptr;
	unsigned long bg = 0, top = 0, bot = 0, sel = 0, fg = 0, field = 0, dim = 0;
	XFontStruct *normal = nullptr, *bold = nullptr, *big = nullptr;

	bool init(Display *d, int scr);
	unsigned long rgb(int r, int g, int b);   // allocate (cached); falls back to the text colour
	int line_height(XFontStruct *f) const { return f->ascent + f->descent; }

private:
	struct Cached { int r, g, b; unsigned long pixel; };
	Cached cache_[48] = {};
	int ncache_ = 0;
};

void fill(Theme &t, Drawable w, Rect r, unsigned long color);
void shadow(Theme &t, Drawable w, Rect r, int thick, bool out);
int text_width(XFontStruct *f, sv s);
void text(Theme &t, Drawable w, XFontStruct *f, int x, int baseline, sv s, unsigned long color);
void text_center(Theme &t, Drawable w, XFontStruct *f, Rect r, sv s, unsigned long color);
// Shorten with "..." until it fits maxw pixels. Returns the number of bytes written to out.
size_t fit_text(XFontStruct *f, sv s, int maxw, char *out, size_t cap);

void button(Theme &t, Drawable w, Rect r, sv label, bool pressed, bool is_default);
void toggle(Theme &t, Drawable w, Rect r, sv label, bool on, bool radio);   // r is the whole row
void field(Theme &t, Drawable w, Rect r, const char *s, size_t n, size_t cur, bool focus);
// The framed picture: sky, then the weather for a WMO code.
void weather_icon(Theme &t, Drawable w, Rect area, long code, bool night);

} // namespace cde
