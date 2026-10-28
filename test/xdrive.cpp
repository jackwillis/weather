// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// xdrive: drive an X application with synthetic events and take screenshots, for GUI tests
// on a virtual X server (Xvfb). No XTest needed: events are sent straight to the window.
//
//   xdrive wait NAME [ms]      wait until a top-level window titled NAME exists (default 20000)
//   xdrive click X Y [BUTTON]  press and release at window coordinates
//   xdrive down|up X Y [BUTTON]  send one button edge (for interrupted gestures)
//   xdrive move X Y            pointer motion
//   xdrive key NAME [ctrl]     press a key by X keysym name (Return, Tab, BackSpace, F5, q, ...)
//   xdrive type TEXT           type printable ASCII
//   xdrive sleep MS
//   xdrive ping                succeed if the X server answers (used to wait for Xvfb)
//   xdrive close               ask the window to close (WM_DELETE_WINDOW), like a window manager would
//   xdrive snap FILE           screenshot of the whole screen: `xwd -root -out FILE`, or $XDRIVE_SNAPCMD FILE
//   xdrive gone NAME [ms]      succeed once the window has disappeared (the app exited)
// Several commands can be chained in one invocation. Exit status 1 on the first failure.
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>

static Display *dpy;
static Window win;
static const char *default_title = "Weather";   // override with XDRIVE_WINDOW

// The app may quit between our events (Ctrl-Q): a send to a vanished window is not an error.
static int on_x_error(Display *, XErrorEvent *e) { return e->error_code == BadWindow ? 0 : (fprintf(stderr, "xdrive: X error %d\n", e->error_code), 0); }

static void msleep(long ms) { struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L}; nanosleep(&ts, nullptr); }

static Window find_window(const char *name)
{
	Window root = DefaultRootWindow(dpy), r, p, *kids = nullptr;
	unsigned n = 0;
	Window found = 0;
	if (XQueryTree(dpy, root, &r, &p, &kids, &n)) {
		for (unsigned i = 0; i < n && !found; i++) {
			char *nm = nullptr;
			if (XFetchName(dpy, kids[i], &nm) && nm) { if (!strcmp(nm, name)) found = kids[i]; XFree(nm); }
		}
		if (kids) XFree(kids);
	}
	return found;
}

static void need_win()
{
	if (win) return;
	for (int t = 0; t < 50 && !win; t++) { win = find_window(default_title); if (!win) msleep(100); }
	if (!win) { fprintf(stderr, "xdrive: no window titled '%s'\n", default_title); exit(1); }
}

static void send_key(KeySym ks, unsigned state)
{
	need_win();
	KeyCode kc = XKeysymToKeycode(dpy, ks);
	if (!kc) { fprintf(stderr, "xdrive: no keycode for keysym 0x%lx\n", static_cast<unsigned long>(ks)); exit(1); }
	if (XkbKeycodeToKeysym(dpy, kc, 0, 0) != ks) state |= ShiftMask;   // this keysym is the shifted one
	for (int down = 1; down >= 0; down--) {
		XEvent e;
		memset(&e, 0, sizeof e);
		e.xkey.type = down ? KeyPress : KeyRelease;
		e.xkey.display = dpy; e.xkey.window = win; e.xkey.root = DefaultRootWindow(dpy);
		e.xkey.subwindow = None; e.xkey.time = CurrentTime; e.xkey.same_screen = True;
		e.xkey.keycode = kc; e.xkey.state = state;
		XSendEvent(dpy, win, True, down ? KeyPressMask : KeyReleaseMask, &e);
		XSync(dpy, False);
		msleep(15);
	}
}

static void send_button(int x, int y, int button, bool press)
{
	need_win();
	XEvent e;
	memset(&e, 0, sizeof e);
	e.xbutton.type = press ? ButtonPress : ButtonRelease;
	e.xbutton.display = dpy; e.xbutton.window = win; e.xbutton.root = DefaultRootWindow(dpy);
	e.xbutton.subwindow = None; e.xbutton.time = CurrentTime; e.xbutton.same_screen = True;
	e.xbutton.x = x; e.xbutton.y = y; e.xbutton.button = static_cast<unsigned>(button);
	e.xbutton.state = press ? 0 : Button1Mask;
	XSendEvent(dpy, win, True, press ? ButtonPressMask : ButtonReleaseMask, &e);
	XSync(dpy, False);
}

static void send_motion(int x, int y)
{
	need_win();
	XEvent e;
	memset(&e, 0, sizeof e);
	e.xmotion.type = MotionNotify;
	e.xmotion.display = dpy; e.xmotion.window = win; e.xmotion.root = DefaultRootWindow(dpy);
	e.xmotion.time = CurrentTime; e.xmotion.same_screen = True; e.xmotion.x = x; e.xmotion.y = y;
	XSendEvent(dpy, win, True, PointerMotionMask, &e);
	XSync(dpy, False);
}

int main(int argc, char **argv)
{
	const char *dn = getenv("DISPLAY");
	if (const char *t = getenv("XDRIVE_WINDOW")) default_title = t;
	XSetErrorHandler(on_x_error);
	dpy = XOpenDisplay(nullptr);
	if (!dpy) { fprintf(stderr, "xdrive: cannot open display %s\n", dn ? dn : "(unset)"); return 1; }
	for (int i = 1; i < argc; i++) {
		const char *c = argv[i];
		if (!strcmp(c, "wait") && i + 1 < argc) {
			const char *name = argv[++i];
			long ms = (i + 1 < argc && atol(argv[i + 1]) > 0) ? atol(argv[++i]) : 20000;
			for (long t = 0; t <= ms; t += 100) { win = find_window(name); if (win) break; msleep(100); }
			if (!win) { fprintf(stderr, "xdrive: window '%s' did not appear\n", name); return 1; }
		} else if (!strcmp(c, "gone") && i + 1 < argc) {
			const char *name = argv[++i];
			long ms = (i + 1 < argc && atol(argv[i + 1]) > 0) ? atol(argv[++i]) : 10000;
			bool gone = false;
			for (long t = 0; t <= ms && !gone; t += 100) { if (!find_window(name)) gone = true; else msleep(100); }
			if (!gone) { fprintf(stderr, "xdrive: window '%s' is still there\n", name); return 1; }
		} else if ((!strcmp(c, "click") || !strcmp(c, "down") || !strcmp(c, "up")) && i + 2 < argc) {
			int x = atoi(argv[++i]), y = atoi(argv[++i]);
			int b = (i + 1 < argc && argv[i + 1][0] >= '1' && argv[i + 1][0] <= '3' && !argv[i + 1][1]) ? atoi(argv[++i]) : 1;
			send_motion(x, y);
			if (strcmp(c, "up")) send_button(x, y, b, true);
			if (!strcmp(c, "click")) msleep(40);
			if (strcmp(c, "down")) send_button(x, y, b, false);
		} else if (!strcmp(c, "move") && i + 2 < argc) {
			send_motion(atoi(argv[i + 1]), atoi(argv[i + 2])); i += 2;
		} else if (!strcmp(c, "key") && i + 1 < argc) {
			KeySym ks = XStringToKeysym(argv[++i]);
			if (ks == NoSymbol) { fprintf(stderr, "xdrive: unknown keysym %s\n", argv[i]); return 1; }
			unsigned st = 0;
			if (i + 1 < argc && !strcmp(argv[i + 1], "ctrl")) { st = ControlMask; i++; }
			send_key(ks, st);
		} else if (!strcmp(c, "type") && i + 1 < argc) {
			for (const char *p = argv[++i]; *p; p++) send_key(static_cast<KeySym>(static_cast<unsigned char>(*p)), 0);
		} else if (!strcmp(c, "ping")) {
			// the display opened: that is the answer
		} else if (!strcmp(c, "close")) {
			need_win();
			XEvent e;
			memset(&e, 0, sizeof e);
			e.xclient.type = ClientMessage; e.xclient.window = win;
			e.xclient.message_type = XInternAtom(dpy, "WM_PROTOCOLS", False);
			e.xclient.format = 32;
			e.xclient.data.l[0] = static_cast<long>(XInternAtom(dpy, "WM_DELETE_WINDOW", False));
			e.xclient.data.l[1] = CurrentTime;
			XSendEvent(dpy, win, False, NoEventMask, &e);
			XSync(dpy, False);
		} else if (!strcmp(c, "sleep") && i + 1 < argc) {
			msleep(atol(argv[++i]));
		} else if (!strcmp(c, "snap") && i + 1 < argc) {
			char cmd[512];
			if (const char *sc = getenv("XDRIVE_SNAPCMD")) snprintf(cmd, sizeof cmd, "%s '%s'", sc, argv[++i]);
			else snprintf(cmd, sizeof cmd, "xwd -root -display '%s' -out '%s'", dn ? dn : ":0", argv[++i]);
			if (system(cmd) != 0) { fprintf(stderr, "xdrive: snapshot failed\n"); return 1; }
		} else {
			fprintf(stderr, "xdrive: bad command '%s'\n", c);
			return 1;
		}
	}
	XCloseDisplay(dpy);
	return 0;
}
