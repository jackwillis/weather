// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// The weather picture, as geometry only. Every UI supplies a small Canvas (the same four calls) so the sun, moon, clouds,
// rain, snow and storm look the same on X11 and on Mac OS 9:
//
//     struct Canvas {
//         void rect(int x, int y, int w, int h, Rgb c);                      // filled rectangle
//         void disc(int cx, int cy, int r, Rgb c);                           // filled circle
//         void line(int x0, int y0, int x1, int y1, int width, Rgb c);       // round-capped line
//         void poly(const ArtPt *p, int n, Rgb c);                          // filled, possibly non-convex polygon
//     };
#pragma once

namespace wx {

struct Rgb { int r, g, b; };
struct ArtPt { int x, y; };
struct ArtRect { int x, y, w, h; };

namespace art {

template <class C>
void cloud(C &c, int cx, int cy, int s100, Rgb body, Rgb edge)
{
	auto sc = [&](int v) { return v * s100 / 100; };
	for (int pass = 0; pass < 2; pass++) {
		int grow = pass == 0 ? 2 : 0;
		Rgb col = pass == 0 ? edge : body;
		c.disc(cx - sc(16), cy + sc(4), sc(12) + grow, col);
		c.disc(cx - sc(2), cy - sc(6), sc(16) + grow, col);
		c.disc(cx + sc(15), cy + sc(2), sc(13) + grow, col);
		c.rect(cx - sc(16), cy + sc(2) - grow, sc(32), sc(14) + 2 * grow, col);
	}
	c.rect(cx - sc(15), cy + sc(2), sc(30), sc(13), body);   // hide the edge line inside the body
}

template <class C>
void sun(C &c, int cx, int cy, int r)
{
	Rgb ray{255, 214, 64}, core{255, 196, 32}, rim{214, 150, 20};
	static const int dir[8][2] = {{100, 0}, {71, 71}, {0, 100}, {-71, 71}, {-100, 0}, {-71, -71}, {0, -100}, {71, -71}};
	for (const auto &d : dir)
		c.line(cx + d[0] * (r + 5) / 100, cy + d[1] * (r + 5) / 100, cx + d[0] * (r + 13) / 100, cy + d[1] * (r + 13) / 100, 3, ray);
	c.disc(cx, cy, r + 1, rim);
	c.disc(cx, cy, r, core);
}

template <class C>
void moon(C &c, int cx, int cy, int r, Rgb sky)
{
	c.disc(cx, cy, r, Rgb{240, 236, 200});
	c.disc(cx + r / 2 + 2, cy - r / 3, r - 1, sky);
}

template <class C>
void drops(C &c, int cx, int y, int n, int len, Rgb col)
{
	for (int i = 0; i < n; i++) {
		int x = cx - 18 + i * (36 / (n > 1 ? n - 1 : 1));
		int yy = y + (i % 2) * 6;
		c.line(x, yy, x - 4, yy + len, 2, col);
	}
}

template <class C>
void flakes(C &c, int cx, int y, Rgb col)
{
	for (int i = 0; i < 5; i++) {
		int x = cx - 20 + i * 10, yy = y + (i % 2) * 9 + 4;
		c.line(x - 3, yy, x + 3, yy, 1, col);
		c.line(x, yy - 3, x, yy + 3, 1, col);
		c.line(x - 2, yy - 2, x + 2, yy + 2, 1, col);
		c.line(x - 2, yy + 2, x + 2, yy - 2, 1, col);
	}
}

// Fill `a` with sky and draw the weather for a WMO code (code < 0: unknown).
template <class C>
void picture_geometry(C &c, ArtRect a, long code, bool night)
{
	Rgb sky = night ? Rgb{38, 50, 86} : Rgb{124, 172, 224};
	c.rect(a.x, a.y, a.w, a.h, sky);
	int cx = a.x + a.w / 2, cy = a.y + a.h / 2 - 4;
	Rgb white{248, 249, 252}, grey{160, 166, 184}, dark{96, 102, 124};
	Rgb edge{110, 124, 160}, blue = night ? Rgb{110, 160, 235} : Rgb{48, 96, 190};

	bool snowy = (code >= 71 && code <= 77) || code == 85 || code == 86;
	bool rainy = (code >= 51 && code <= 67) || (code >= 80 && code <= 82);
	bool storm = code >= 95 && code <= 99;
	bool foggy = code == 45 || code == 48;

	if (code == 0 || code == 1) {
		if (night) moon(c, cx, cy, 24, sky);
		else sun(c, cx, cy, 22);
		if (night) {                                               // a few stars
			c.rect(a.x + 14, a.y + 14, 2, 2, white);
			c.rect(a.x + a.w - 22, a.y + 22, 2, 2, white);
			c.rect(a.x + a.w - 30, a.y + a.h - 24, 2, 2, white);
		}
	} else if (code == 2) {
		if (night) moon(c, cx - 14, cy - 14, 18, sky); else sun(c, cx - 14, cy - 14, 16);
		cloud(c, cx + 4, cy + 10, 100, white, edge);
	} else if (foggy) {
		for (int i = 0; i < 4; i++)
			c.line(cx - 28 + (i % 2) * 8, cy - 16 + i * 14, cx + 26 - ((i + 1) % 2) * 8, cy - 16 + i * 14, 4, grey);
	} else if (code == 3) {
		cloud(c, cx + 8, cy - 8, 80, grey, edge);
		cloud(c, cx - 4, cy + 8, 100, white, edge);
	} else if (storm) {
		cloud(c, cx, cy - 10, 100, dark, edge);
		ArtPt bolt[6] = {{cx + 4, cy + 6}, {cx - 8, cy + 26}, {cx - 1, cy + 26}, {cx - 6, cy + 42}, {cx + 10, cy + 20}, {cx + 3, cy + 20}};
		c.poly(bolt, 6, Rgb{255, 214, 64});
		if (code >= 96) drops(c, cx - 12, cy + 12, 2, 8, blue);
	} else if (snowy) {
		cloud(c, cx, cy - 10, 100, white, edge);
		flakes(c, cx, cy + 14, white);
	} else if (rainy) {
		bool heavy = code == 55 || code == 65 || code == 67 || code == 82;
		cloud(c, cx, cy - 10, 100, grey, edge);
		drops(c, cx, cy + 14, heavy ? 6 : 4, heavy ? 14 : 10, blue);
	} else {
		cloud(c, cx, cy, 100, white, edge);                        // unknown: a plain cloud
	}
}


// Map the complete 100x100 scene into a smaller centered square. A two-pixel
// margin keeps minimum-width lines and their caps inside even at tiny scales.
// Arrays retain their compile-time size; no heap or polygon-size limit needed.
template <class C>
struct ScaledCanvas {
	C &canvas;
	int x, y, span;
	int scale(int v) const { return v * span / 100; }
	void rect(int xx, int yy, int w, int h, Rgb color) {
		canvas.rect(x + scale(xx), y + scale(yy), scale(xx + w) - scale(xx), scale(yy + h) - scale(yy), color);
	}
	void disc(int xx, int yy, int r, Rgb color) { canvas.disc(x + scale(xx), y + scale(yy), scale(r), color); }
	void line(int x0, int y0, int x1, int y1, int width, Rgb color) {
		int pen = scale(width);
		canvas.line(x + scale(x0), y + scale(y0), x + scale(x1), y + scale(y1), pen > 0 ? pen : 1, color);
	}
	template <unsigned N>
	void poly(const ArtPt (&points)[N], int n, Rgb color) {
		if (n < 0 || static_cast<unsigned>(n) > N) return;
		ArtPt mapped[N];
		for (int i = 0; i < n; i++) mapped[i] = {x + scale(points[i].x), y + scale(points[i].y)};
		canvas.poly(mapped, n, color);
	}
};

// Draws through another Canvas with every colour moved `pct` percent of the way toward `ground` (the window background), so a
// stale picture reads as faded like the stale text beside it (#62). No state, no allocation.
template <class C>
struct DimCanvas {
	C &canvas;
	Rgb ground;
	int pct;
	Rgb mix(Rgb c) const {
		return Rgb{c.r + (ground.r - c.r) * pct / 100, c.g + (ground.g - c.g) * pct / 100, c.b + (ground.b - c.b) * pct / 100};
	}
	void rect(int x, int y, int w, int h, Rgb c) { canvas.rect(x, y, w, h, mix(c)); }
	void disc(int x, int y, int r, Rgb c) { canvas.disc(x, y, r, mix(c)); }
	void line(int x0, int y0, int x1, int y1, int width, Rgb c) { canvas.line(x0, y0, x1, y1, width, mix(c)); }
	void poly(const ArtPt *p, int n, Rgb c) { canvas.poly(p, n, mix(c)); }
};

// Larger areas keep the original geometry. Small areas fit the whole scene;
// empty areas draw nothing, and areas below five pixels show only sky.
template <class C>
void picture(C &c, ArtRect a, long code, bool night)
{
	if (a.w <= 0 || a.h <= 0) return;
	if (a.w >= 100 && a.h >= 100) { picture_geometry(c, a, code, night); return; }
	Rgb sky = night ? Rgb{38, 50, 86} : Rgb{124, 172, 224};
	c.rect(a.x, a.y, a.w, a.h, sky);
	int side = a.w < a.h ? a.w : a.h;
	if (side < 5) return;
	ScaledCanvas<C> scaled{c, a.x + (a.w - side) / 2 + 2, a.y + (a.h - side) / 2 + 2, side - 4};
	picture_geometry(scaled, ArtRect{0, 0, 100, 100}, code, night);
}

} // namespace art
} // namespace wx
