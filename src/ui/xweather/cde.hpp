// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// CDE-look drawing primitives on plain Xlib (immediate mode: callers redraw everything
// into a Pixmap, then copy it to the window). Not the Motif API: no Xt, no libXm.
// Resources (xrdb): xbelike.background, .foreground, .font, .boldFont, .bigFont.
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
// Draw a 1-bit bitmap (MSB first, rows padded to rowbytes, 1 = ink) at x,y as horizontal runs in one colour.
void bitmap1(Theme &t, Drawable w, int x, int y, const unsigned char *bits, int bw, int bh, int rowbytes, unsigned long color);
void shadow(Theme &t, Drawable w, Rect r, int thick, bool out);
int text_width(XFontStruct *f, sv s);
void text(Theme &t, Drawable w, XFontStruct *f, int x, int baseline, sv s, unsigned long color);
void text_center(Theme &t, Drawable w, XFontStruct *f, Rect r, sv s, unsigned long color);
// Shorten with "..." until it fits maxw pixels. Returns the number of bytes written to out.
size_t fit_text(XFontStruct *f, sv s, int maxw, char *out, size_t cap);

void button(Theme &t, Drawable w, Rect r, sv label, bool pressed, bool is_default);
void toggle(Theme &t, Drawable w, Rect r, sv label, bool on, bool radio);   // r is the whole row
// Horizontal text viewport shared by rendering and mouse hit testing.
size_t field_start(XFontStruct *font, sv text, size_t cursor, int width);
size_t field_position(XFontStruct *font, sv text, size_t cursor, int width, int x);
void field(Theme &t, Drawable w, Rect r, const char *s, size_t n, size_t cur, bool focus);
// The framed picture: sky, then the weather for a WMO code.
void weather_icon(Theme &t, Drawable w, Rect area, long code, bool night, bool faded = false);

} // namespace cde
