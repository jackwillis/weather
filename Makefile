# Portable between BSD make and GNU make: plain variables and explicit rules only.
# make            build for this machine into build/host/
# make ppc        cross-build for NetBSD/macppc into build/macppc/ (needs toolchain, see README)
# make check      unit tests + feature tests on this machine
# make smoke-maul  cross-build, push to maul and test the real binary there
# make clean

VERSION = 0.1.0
BEARSSL = spike/bearssl

# cross toolchain (see ~/scratch/netbsd-xc/README-repro.md)
XROOT   = $(HOME)/scratch/netbsd-xc
XPFX    = $(XROOT)/tools2/bin/powerpc--netbsd
SYSROOT = $(XROOT)/sysroot

WARN    = -Wall -Wextra -Wno-unused-parameter
CXXFLAGS_COMMON = -std=c++17 -O2 -g $(WARN) -fno-exceptions -fno-rtti -DWEATHER_VERSION='"$(VERSION)"' -Isrc -I$(BEARSSL)/inc

SRCS = src/log.cpp src/fetch.cpp src/net_posix.cpp src/tls.cpp src/nocxx.cpp
CORE_HDRS = src/status.hpp src/text.hpp src/json.hpp src/http.hpp src/forecast.hpp src/provider.hpp \
    src/openmeteo.hpp src/net.hpp src/log.hpp src/fetch.hpp src/config.hpp src/version.hpp

all: host
host: build/host/weather
ppc: build/macppc/weather build/macppc/weather.debug

build/host/weather: src/main.cpp $(SRCS) $(CORE_HDRS) src/anchors.h $(BEARSSL)/buildhost/libbearssl.a
	@mkdir -p build/host
	$(CXX) $(CXXFLAGS_COMMON) -o $@ src/main.cpp $(SRCS) $(BEARSSL)/buildhost/libbearssl.a

build/macppc/weather.debug: src/main.cpp $(SRCS) $(CORE_HDRS) src/anchors.h $(BEARSSL)/buildppc/libbearssl.a
	@mkdir -p build/macppc
	$(XPFX)-g++ --sysroot=$(SYSROOT) $(CXXFLAGS_COMMON) -Wl,--as-needed -o $@ src/main.cpp $(SRCS) $(BEARSSL)/buildppc/libbearssl.a

build/macppc/weather: build/macppc/weather.debug
	$(XPFX)-strip -o $@ build/macppc/weather.debug

build/host/unittest: test/unittest.cpp src/log.cpp src/fetch.cpp $(CORE_HDRS)
	@mkdir -p build/host
	$(CXX) $(CXXFLAGS_COMMON) -DFIXTURE_DIR='"test/fixtures"' -o $@ test/unittest.cpp src/log.cpp src/fetch.cpp

$(BEARSSL)/buildhost/libbearssl.a:
	$(MAKE) -C $(BEARSSL) BUILD=buildhost CONF=Unix lib

$(BEARSSL)/buildppc/libbearssl.a:
	$(MAKE) -C $(BEARSSL) BUILD=buildppc CONF=Unix CC="$(XPFX)-gcc --sysroot=$(SYSROOT)" \
	    LD="$(XPFX)-gcc --sysroot=$(SYSROOT)" AR=$(XPFX)-ar CFLAGS="-W -Wall -Os -fPIC" LDFLAGS="" lib

check: build/host/unittest build/host/weather
	build/host/unittest
	sh test/feature.sh build/host/weather

smoke-maul: ppc
	sh test/smoke-maul.sh build/macppc/weather

clean:
	rm -rf build

.PHONY: all host ppc check smoke-maul clean
