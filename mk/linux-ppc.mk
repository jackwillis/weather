# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Supplemental big-endian Linux execution under QEMU; not NetBSD acceptance.
CXX = powerpc-linux-gnu-g++
STRIP = powerpc-linux-gnu-strip -o $@ $<
BEARSSL_MAKEVARS = CC=powerpc-linux-gnu-gcc AR=powerpc-linux-gnu-ar
EXTRA_CXXFLAGS = -Werror -fsanitize=undefined -fsanitize-undefined-trap-on-error
EXTRA_LDFLAGS =
CHECK_TARGET = check-ppc
