# Cross build for NetBSD/macppc from Linux. Toolchain: ~/scratch/netbsd-xc (see its README-repro.md).
XROOT   = $(HOME)/scratch/netbsd-xc
XPFX    = $(XROOT)/tools2/bin/powerpc--netbsd
SYSROOT = $(XROOT)/sysroot
CXX     = $(XPFX)-g++ --sysroot=$(SYSROOT)
STRIP   = $(XPFX)-strip -o $@ $<
BEARSSL_MAKEVARS = CC="$(XPFX)-gcc --sysroot=$(SYSROOT)" LD="$(XPFX)-gcc --sysroot=$(SYSROOT)" \
                   AR=$(XPFX)-ar CFLAGS="-W -Wall -Os -fPIC" LDFLAGS=""
EXTRA_CXXFLAGS =
EXTRA_LDFLAGS  = -Wl,--as-needed
CHECK_TARGET   = check-cross
# X11 comes from the sysroot (xbase/xcomp sets); the rpath is required on NetBSD.
X11_CXXFLAGS = -I$(SYSROOT)/usr/X11R7/include
X11_LIBS     = -L$(SYSROOT)/usr/X11R7/lib -Wl,-rpath,/usr/X11R7/lib -lX11
XWEATHER_TARGETS = $(BUILD)/xweather
