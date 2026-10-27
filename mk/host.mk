# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Native build on the development machine (Linux). Runs the tests directly.
CXX         = c++
STRIP       = cp $< $@
BEARSSL_MAKEVARS =
EXTRA_CXXFLAGS = -Werror
EXTRA_LDFLAGS  =
CHECK_TARGET   = check-native
# xweather needs the X11 development headers (Fedora: libX11-devel); skipped when they are absent.
ifneq ($(wildcard /usr/include/X11/Xlib.h),)
X11_CXXFLAGS =
X11_LIBS     = -lX11
XWEATHER_TARGETS = $(BUILD)/xweather
CHECK_TARGET   = check-native check-gui
endif
