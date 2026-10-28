// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Real Xlib pixels: long field text and its caret must not escape the widget.
#include "ui/xweather/cde.hpp"
#include <cstdio>
#include <cstring>
#include <ctime>
static int checks, failures;
static void check(bool ok, const char *name) { checks++; if (!ok) { failures++; printf("FAIL field: %s\n", name); } }
int main()
{
	XFontStruct mono{}; mono.min_char_or_byte2 = 0; mono.max_char_or_byte2 = 255;
	mono.min_bounds.width = mono.max_bounds.width = 6; mono.all_chars_exist = True;
	wx::sv digits("0123456789");
	check(cde::field_start(&mono, digits, 10, 20) == 7, "end scrolls into view");
	check(cde::field_start(&mono, digits, 0, 20) == 0, "Home restores start");
	check(cde::field_start(&mono, digits, 2, 20) == 0, "short prefix stays put");
	check(cde::field_start(&mono, digits, 100, 20) == 7, "cursor clamped");
	check(cde::field_start(&mono, digits, 10, 0) == 10, "zero width safe");
	check(cde::field_start(&mono, wx::sv{}, 0, 20) == 0, "empty value safe");
	check(cde::field_position(&mono, digits, 10, 20, 0) == 7, "click uses visible start");
	check(cde::field_position(&mono, digits, 10, 20, 4) == 8, "glyph midpoint");
	check(cde::field_position(&mono, digits, 10, 20, 10) == 9, "visible middle");
	check(cde::field_position(&mono, digits, 10, 20, 999) == 10, "right click clamped");
	check(cde::field_position(&mono, digits, 10, 20, -99) == 7, "left click clamped");
	check(cde::field_position(&mono, digits, 10, 0, 99) == 10, "zero width click safe");
	XCharStruct glyphs[256] = {};
	for (auto &glyph : glyphs) glyph.width = 6;
	glyphs['W'].width = 9; glyphs['I'].width = 2;
	XFontStruct proportional = mono; proportional.per_char = glyphs;
	proportional.min_bounds.width = 2; proportional.max_bounds.width = 9;
	check(cde::field_start(&proportional, "WIWI", 4, 12) == 2, "proportional scroll");
	check(cde::field_position(&proportional, "WIWI", 4, 12, 6) == 3, "proportional click");
	check(cde::field_position(&proportional, "WIWI", 4, 12, 11) == 4, "proportional end");
	Display *display = nullptr;
	for (int i = 0; i < 100 && !display; i++) {
		display = XOpenDisplay(nullptr);
		if (!display) { timespec delay{0, 20000000}; nanosleep(&delay, nullptr); }
	}
	if (!display) { puts("field: cannot open test display"); return 1; }
	cde::Theme theme;
	if (!theme.init(display, DefaultScreen(display))) { puts("field: cannot initialize theme"); return 1; }
	Pixmap pixmap = XCreatePixmap(display, DefaultRootWindow(display), 440, 100, DefaultDepth(display, DefaultScreen(display)));
	unsigned long guard = theme.rgb(255, 0, 255);
	const char *name = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu";
	for (int focus = 0; focus < 2; focus++) {
		cde::fill(theme, pixmap, {0, 0, 440, 100}, guard);
		cde::field(theme, pixmap, {20, 20, 200, 24}, name, strlen(name), strlen(name), focus != 0);
		XImage *image = XGetImage(display, pixmap, 0, 0, 440, 100, AllPlanes, ZPixmap);
		bool intact = image != nullptr;
		if (image) {
			for (int y = 0; y < 100; y++) for (int x = 0; x < 440; x++)
				if ((x < 18 || x >= 222 || y < 18 || y >= 46) && XGetPixel(image, x, y) != guard) intact = false;
			if (focus) {
				size_t start = cde::field_start(theme.normal, name, strlen(name), 188);
				int caret = 26 + cde::text_width(theme.normal, wx::sv(name).substr(start));
				check(caret < 214 && XGetPixel(image, caret, 24) == theme.fg, "end caret visible inside field");
			}
			XDestroyImage(image);
		}
		check(intact, "text and focus stay inside widget bounds");
	}
	// The widget's temporary clip must not constrain subsequent painting.
	XSetForeground(display, theme.gc, theme.fg);
	XDrawPoint(display, pixmap, theme.gc, 0, 0);
	XImage *point = XGetImage(display, pixmap, 0, 0, 1, 1, AllPlanes, ZPixmap);
	check(point && XGetPixel(point, 0, 0) == theme.fg, "clip reset after widget");
	if (point) XDestroyImage(point);
	XFreePixmap(display, pixmap);
	XFreeGC(display, theme.gc);
	if (theme.big != theme.bold && theme.big != theme.normal) XFreeFont(display, theme.big);
	if (theme.bold != theme.normal) XFreeFont(display, theme.bold);
	XFreeFont(display, theme.normal);
	XCloseDisplay(display);
	printf("field: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
