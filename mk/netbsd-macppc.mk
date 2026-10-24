# Cross build for NetBSD/macppc from Linux. Toolchain: ~/scratch/netbsd-xc (see its README-repro.md).
XROOT   = $(HOME)/scratch/netbsd-xc
XPFX    = $(XROOT)/tools2/bin/powerpc--netbsd
SYSROOT = $(XROOT)/sysroot
CXX     = $(XPFX)-g++ --sysroot=$(SYSROOT)
STRIP   = $(XPFX)-strip -o $@ $<
BEARSSL_BUILD    = buildppc
BEARSSL_MAKEVARS = CC="$(XPFX)-gcc --sysroot=$(SYSROOT)" LD="$(XPFX)-gcc --sysroot=$(SYSROOT)" \
                   AR=$(XPFX)-ar CFLAGS="-W -Wall -Os -fPIC" LDFLAGS=""
EXTRA_CXXFLAGS =
EXTRA_LDFLAGS  = -Wl,--as-needed
CHECK_TARGET   = check-cross
