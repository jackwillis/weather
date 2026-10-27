# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Native 32-bit development checks on x86 Linux (requires g++ multilib).
# This catches long-width regressions; it is not a big-endian hardware substitute.
include mk/host.mk
CXX = c++ -m32
BEARSSL_MAKEVARS = CC="cc -m32"
EXTRA_CXXFLAGS += -fsanitize=undefined -fsanitize-undefined-trap-on-error
XWEATHER_TARGETS =
X11_CXXFLAGS =
X11_LIBS =
CHECK_TARGET = check-native
