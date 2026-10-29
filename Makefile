# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# GNU make required (BSD hosts: gmake). One fragment per target in mk/.
#
#   make                      build for this machine           -> build/host/
#   make TARGET=netbsd-macppc cross-build for NetBSD/macppc    -> build/netbsd-macppc/
#   make check                unit + feature tests (host), see mk/host.mk
#   make lint                 portable layers must not include OS headers
#   make check-maul           cross-build the unit tests and run them on maul (big-endian ground truth)
#   make check-san            unit + robustness + feature tests under ASan/UBSan (mk/host-san.mk)
#   make install              DESTDIR/PREFIX/MANDIR honoured; `make plist` lists the files; `make dist`, `make distcheck`
#   make package-netbsd       binary package for maul (pkg_create runs on maul): build/pkg/weather-VERSION.tgz
#   make smoke-maul           cross-build, push to maul, test the real binary there
#   make clean
# Shortcut: `make ppc` = make TARGET=netbsd-macppc.

TARGET  ?= host
VERSION  = 0.1.0
BEARSSL ?= third_party/bearssl

include mk/$(TARGET).mk

# Explicit test-only builds never share objects or release paths with production.
TEST_HOOKS ?= 0
ifneq ($(TEST_HOOKS),0)
ifneq ($(TEST_HOOKS),1)
$(error TEST_HOOKS must be 0 or 1)
endif
endif
ifeq ($(TEST_HOOKS),1)
ifneq ($(filter install package-netbsd dist distcheck,$(MAKECMDGOALS)),)
$(error test-hook builds cannot be installed, packaged or distributed)
endif
endif
BUILD = build/$(TARGET)$(if $(filter 1,$(TEST_HOOKS)),-test-hooks)
TEST_HOOK_FLAGS = $(if $(filter 1,$(TEST_HOOKS)),-DWEATHER_TEST_HOOKS)
OBJ   = $(BUILD)/obj
WARN  = -Wall -Wextra -Wno-unused-parameter
CXXFLAGS_ALL = -std=c++17 -O2 -g $(WARN) $(EXTRA_CXXFLAGS) $(X11_CXXFLAGS) -fno-exceptions -fno-rtti \
    -MMD -MP -DWEATHER_VERSION='"$(VERSION)"' -Isrc -I$(BEARSSL)/inc $(CXXFLAGS) $(TEST_HOOK_FLAGS)

BEARSSL_OUT = $(BUILD)/bearssl
BEARSSL_LIB = $(BEARSSL_OUT)/libbearssl.a

# Layers (see docs/ARCHITECTURE.md). Platform is chosen by the target; only posix exists so far.
PLATFORM     = posix
CORE_SRCS    = src/core/fetch.cpp
TLS_SRCS     = src/tls/tls.cpp
PLAT_SRCS    = src/platform/$(PLATFORM)/net_posix.cpp src/platform/$(PLATFORM)/log_posix.cpp \
               src/platform/$(PLATFORM)/settings_posix.cpp src/platform/$(PLATFORM)/cache_posix.cpp src/platform/$(PLATFORM)/nocxx.cpp
CLI_SRCS     = src/cli/main.cpp
XW_SRCS      = src/ui/xweather/main.cpp src/ui/xweather/cde.cpp
WEATHER_OBJS = $(patsubst src/%.cpp,$(OBJ)/%.o,$(CORE_SRCS) $(TLS_SRCS) $(PLAT_SRCS) $(CLI_SRCS))
XW_OBJS      = $(patsubst src/%.cpp,$(OBJ)/%.o,$(CORE_SRCS) $(TLS_SRCS) $(PLAT_SRCS) $(XW_SRCS))
TEST_OBJS    = $(OBJ)/app/preferences.o $(OBJ)/test/unittest.o $(OBJ)/core/fetch.o $(OBJ)/platform/$(PLATFORM)/log_posix.o \
               $(OBJ)/platform/$(PLATFORM)/net_posix.o $(OBJ)/platform/$(PLATFORM)/settings_posix.o $(OBJ)/platform/$(PLATFORM)/cache_posix.o

all: $(BUILD)/weather $(XWEATHER_TARGETS)
ppc:
	$(MAKE) TARGET=netbsd-macppc all

$(BUILD)/weather.debug: $(WEATHER_OBJS) $(BEARSSL_LIB)
	$(CXX) $(EXTRA_LDFLAGS) -o $@ $(WEATHER_OBJS) $(BEARSSL_LIB)

$(BUILD)/weather: $(BUILD)/weather.debug
	$(STRIP)

$(BUILD)/xweather.debug: $(XW_OBJS) $(BEARSSL_LIB)
	$(CXX) $(EXTRA_LDFLAGS) -o $@ $(XW_OBJS) $(BEARSSL_LIB) $(X11_LIBS)

$(BUILD)/xweather: $(BUILD)/xweather.debug
	$(STRIP)

# Separate diagnostic executable: production links no fixture generator.
$(OBJ)/ui/xweather/search_runner.o: src/ui/xweather/main.cpp $(OBJ)/.flags
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_ALL) -DWEATHER_XWEATHER_LIBRARY -c -o $@ $<

$(BUILD)/xweather-search-test: $(filter-out $(OBJ)/ui/xweather/main.o,$(XW_OBJS)) $(OBJ)/ui/xweather/search_runner.o $(OBJ)/test/xweather_search.o $(BEARSSL_LIB)
	$(CXX) $(EXTRA_LDFLAGS) -o $@ $(filter-out $(OBJ)/ui/xweather/main.o,$(XW_OBJS)) $(OBJ)/ui/xweather/search_runner.o $(OBJ)/test/xweather_search.o $(BEARSSL_LIB) $(X11_LIBS)


$(BUILD)/xweather-stale-test: $(filter-out $(OBJ)/ui/xweather/main.o,$(XW_OBJS)) $(OBJ)/ui/xweather/search_runner.o $(OBJ)/test/xweather_stale.o $(BEARSSL_LIB)
	$(CXX) $(EXTRA_LDFLAGS) -o $@ $^ $(X11_LIBS)


$(BUILD)/cde-text-test: $(OBJ)/test/cde_text.o $(OBJ)/ui/xweather/cde.o $(OBJ)/platform/$(PLATFORM)/nocxx.o
	$(CXX) $(EXTRA_LDFLAGS) -o $@ $^ $(X11_LIBS)

$(BUILD)/cde-field-test: $(OBJ)/test/cde_field.o $(OBJ)/ui/xweather/cde.o $(OBJ)/platform/$(PLATFORM)/nocxx.o
	$(CXX) $(EXTRA_LDFLAGS) -o $@ $^ $(X11_LIBS)

$(BUILD)/xdrive: $(OBJ)/test/xdrive.o
	$(CXX) $(EXTRA_LDFLAGS) -o $@ $< $(X11_LIBS)

$(BUILD)/connect-budget-test: $(OBJ)/test/connect_budget.o $(OBJ)/platform/$(PLATFORM)/log_posix.o $(OBJ)/platform/$(PLATFORM)/nocxx.o
	$(CXX) $(EXTRA_LDFLAGS) -o $@ $^

# Optional live TLS compatibility probe, never part of the offline check suite.
$(BUILD)/tls-live: $(OBJ)/test/tls_live.o $(OBJ)/tls/tls.o $(OBJ)/platform/$(PLATFORM)/net_posix.o $(OBJ)/platform/$(PLATFORM)/log_posix.o $(OBJ)/platform/$(PLATFORM)/nocxx.o $(BEARSSL_LIB)
	$(CXX) $(EXTRA_LDFLAGS) -o $@ $^

$(BUILD)/robust: $(OBJ)/test/robust.o $(OBJ)/app/preferences.o $(BEARSSL_LIB)
	$(CXX) $(EXTRA_LDFLAGS) -o $@ $< $(OBJ)/app/preferences.o $(BEARSSL_LIB)

$(BUILD)/unittest: $(TEST_OBJS) $(BEARSSL_LIB)
	$(CXX) $(EXTRA_LDFLAGS) -o $@ $(TEST_OBJS) $(BEARSSL_LIB)

# Objects depend on the exact compiler command: change a flag (or the toolchain) and everything rebuilds.
# Written by a shell recipe, not $(file ...), so GNU make 3.81 (macOS) works too. Caveat: 3.81 compares file times
# in whole seconds, so changing flags within the same second as the previous build can leave objects unrebuilt.
$(OBJ)/.flags: export FLAGS_LINE = $(CXX) $(CXXFLAGS_ALL)
$(OBJ)/.flags: FORCE
	@mkdir -p $(OBJ)
	@printf '%s\n' "$$FLAGS_LINE" > $@.new
	@if cmp -s $@.new $@; then rm -f $@.new; else mv $@.new $@; fi
FORCE:

$(OBJ)/%.o: src/%.cpp $(OBJ)/.flags
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_ALL) -c -o $@ $<

$(OBJ)/test/%.o: test/%.cpp $(OBJ)/.flags
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_ALL) -DFIXTURE_DIR='"test/fixtures"' -c -o $@ $<

$(BEARSSL_LIB):
	$(MAKE) -C $(BEARSSL) BUILD=$(abspath $(BEARSSL_OUT)) CONF=Unix $(BEARSSL_MAKEVARS) lib

# ---- trust anchors: anchors.h is generated from isrg-roots.pem (needs the host brssl tool)
$(BUILD)/bearssl/brssl:
	$(MAKE) -C $(BEARSSL) BUILD=$(abspath $(BEARSSL_OUT)) CONF=Unix $(BEARSSL_MAKEVARS) $(abspath $(BEARSSL_OUT))/brssl

anchors:
	$(MAKE) TARGET=host build/host/bearssl/brssl
	sh tools/gen-anchors.sh build/host/bearssl/brssl > src/tls/anchors.h.new && mv src/tls/anchors.h.new src/tls/anchors.h

# Certificate verification, offline: the real tls.cpp against a local TLS server with generated certificates (needs openssl, python3).
check-tls: $(BEARSSL_LIB) $(OBJ)/core/fetch.o $(OBJ)/platform/$(PLATFORM)/net_posix.o $(OBJ)/platform/$(PLATFORM)/log_posix.o
	$(MAKE) TARGET=host build/host/bearssl/brssl
	sh test/tls_local.sh "$(CXX)" $(BUILD) build/host/bearssl/brssl "$(EXTRA_CXXFLAGS)" "$(EXTRA_LDFLAGS)"

check-anchors:
	$(MAKE) TARGET=host build/host/bearssl/brssl
	sh tools/check-anchors.sh build/host/bearssl/brssl
	sh test/anchors.sh build/host/bearssl/brssl

# ---- install / release (DESTDIR, PREFIX, MANDIR honoured; works for a cross TARGET too)
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
MANDIR  ?= $(PREFIX)/share/man
EXDIR   ?= $(PREFIX)/share/examples/weather
DOCDIR  ?= $(PREFIX)/share/doc/weather
INSTALL ?= install
DISTNAME = weather-$(VERSION)

install: all
	$(INSTALL) -d -m 755 $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man1 $(DESTDIR)$(MANDIR)/man5 $(DESTDIR)$(EXDIR) $(DESTDIR)$(DOCDIR)
	$(INSTALL) -m 755 $(BUILD)/weather $(DESTDIR)$(BINDIR)/weather
	$(INSTALL) -m 644 man/weather.1 $(DESTDIR)$(MANDIR)/man1/weather.1
	$(INSTALL) -m 644 man/weatherrc.5 $(DESTDIR)$(MANDIR)/man5/weatherrc.5
	$(INSTALL) -m 644 share/weatherrc.example $(DESTDIR)$(EXDIR)/weatherrc
	$(INSTALL) -m 644 LICENSE THIRD_PARTY.md $(DESTDIR)$(DOCDIR)/
	@if [ -n "$(XWEATHER_TARGETS)" ]; then \
	    echo "$(INSTALL) -m 755 $(BUILD)/xweather $(DESTDIR)$(BINDIR)/xweather"; \
	    $(INSTALL) -m 755 $(BUILD)/xweather $(DESTDIR)$(BINDIR)/xweather && \
	    $(INSTALL) -m 644 man/xweather.1 $(DESTDIR)$(MANDIR)/man1/xweather.1; fi

# The files `make install` would install, relative to PREFIX: the basis of a package PLIST.
plist:
	@rm -rf $(BUILD)/plist-stage
	@$(MAKE) -s install DESTDIR=$(abspath $(BUILD)/plist-stage) PREFIX=$(PREFIX) MANDIR=$(MANDIR) >/dev/null
	@cd $(BUILD)/plist-stage$(PREFIX) && find . -type f | sed 's|^\./||' | sort

# Source tarball of the current commit (refuses a dirty tree), reproducible: same commit, same bytes.
dist:
	@git diff --quiet HEAD -- || { echo "dist: uncommitted changes: commit first"; exit 1; }
	@mkdir -p build/dist
	git archive --format=tar --prefix=$(DISTNAME)/ HEAD | gzip -9n > build/dist/$(DISTNAME).tar.gz
	@cd build/dist && h=$$(sha512sum $(DISTNAME).tar.gz | cut -d' ' -f1) && \
	    echo "SHA512 ($(DISTNAME).tar.gz) = $$h" > $(DISTNAME).tar.gz.sha512 && \
	    echo "Size ($(DISTNAME).tar.gz) = $$(wc -c < $(DISTNAME).tar.gz) bytes" >> $(DISTNAME).tar.gz.sha512 && \
	    cat $(DISTNAME).tar.gz.sha512

# NetBSD binary package for maul: cross-build, stage, pkg_create on maul -> build/pkg/weather-VERSION.tgz
package-netbsd:
	sh tools/mkpkg-netbsd.sh

# OS 9 compiler probe (needs podman/docker; pulls Retro68's image once): does the portable core compile there?
probe-os9:
	sh tools/probe-os9.sh

# Native compiler acceptance plus negative checks of our private-record guards.
check-ot-layout:
	$(CONTAINER) run --rm -v $(CURDIR):/src:Z -w /src $(RETRO68_IMAGE) sh test/ot-layout.sh /Retro68-build/toolchain/bin/powerpc-apple-macos-g++

# Release builds pin RETRO68_IMAGE by digest (ghcr.io/autc04/retro68@sha256:...) and may use CONTAINER=docker.
CONTAINER    ?= podman
RETRO68_IMAGE ?= ghcr.io/autc04/retro68
# Mac OS 9 build (PowerPC PEF app "Weather") with Retro68 in a container. Output: build/os9/Weather.{APPL,bin,dsk}.
# Deliver to the Mac with tools/deliver-os9.sh (see README). Needs no Apple files.
os9:
	mkdir -p build/os9
	$(CONTAINER) run --rm -v $(CURDIR):/src:Z -w /src/build/os9 $(RETRO68_IMAGE) sh -c \
	  'cmake /src/os9 -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake >/dev/null && make'

# Prove the tarball is complete and reproducible: unpack it elsewhere, build, run the tests, rebuild the tarball.
distcheck: dist
	@rm -rf build/distcheck && mkdir -p build/distcheck
	tar -xzf build/dist/$(DISTNAME).tar.gz -C build/distcheck
	$(MAKE) -C build/distcheck/$(DISTNAME) check-native
	@cp build/dist/$(DISTNAME).tar.gz build/dist/first.tar.gz
	@$(MAKE) -s dist >/dev/null
	@cmp build/dist/first.tar.gz build/dist/$(DISTNAME).tar.gz && echo "distcheck: tarball is reproducible" && rm -f build/dist/first.tar.gz

# Host-test the actual OS9 glue against test-owned system API stand-ins.
$(BUILD)/ot-shutdown-test: test/ot_shutdown.cpp src/platform/os9/ot_glue.cc src/platform/os9/ot_min.h test/os9-stubs/MacTypes.h test/os9-stubs/Gestalt.h src/platform/posix/nocxx.cpp FORCE
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS_ALL) -Wno-multichar -Wno-address -Itest/os9-stubs $(EXTRA_LDFLAGS) -o $@ test/ot_shutdown.cpp src/platform/posix/nocxx.cpp

# ---- tests
check: $(CHECK_TARGET)
PPC_RUN ?= qemu-ppc -L /usr/powerpc-linux-gnu
$(BUILD)/ppc-runtime: $(OBJ)/test/ppc_runtime.o
	$(CXX) $(EXTRA_LDFLAGS) -o $@ $<

# No binfmt registration or privileged emulator setup is required.
check-ppc: $(BUILD)/ppc-runtime $(BUILD)/unittest $(BUILD)/robust
	$(PPC_RUN) $(BUILD)/ppc-runtime
	$(PPC_RUN) $(BUILD)/unittest
	$(PPC_RUN) $(BUILD)/robust
.PHONY: check-ppc


# Exercise both configurations, preserving production defaults and release artifacts.
check-test-hooks: $(BUILD)/weather
	$(MAKE) TARGET=$(TARGET) TEST_HOOKS=1 build/$(TARGET)-test-hooks/unittest build/$(TARGET)-test-hooks/weather build/$(TARGET)-test-hooks/connect-budget-test
	build/$(TARGET)-test-hooks/unittest
	build/$(TARGET)-test-hooks/connect-budget-test
	TEST_HOOKS_ENABLED=1 sh test/feature.sh build/$(TARGET)-test-hooks/weather
	sh test/test-builds.sh $(BUILD)/weather build/$(TARGET)-test-hooks/weather
check-native: lint check-anchors check-tls $(BUILD)/unittest $(BUILD)/robust $(BUILD)/weather $(BUILD)/connect-budget-test $(BUILD)/ot-shutdown-test
	sh test/ot-shutdown.sh $(BUILD)/ot-shutdown-test
	$(BUILD)/unittest
	$(BUILD)/connect-budget-test
	$(BUILD)/robust
	TEST_HOOKS_ENABLED=$(TEST_HOOKS) sh test/feature.sh $(BUILD)/weather
	python3 test/cache_cli.py $(BUILD)/weather
	python3 test/fixture_startup.py
ifeq ($(TEST_HOOKS),0)
	$(MAKE) TARGET=$(TARGET) check-test-hooks
endif
	python3 test/seed_provision.py
	python3 test/session_edits.py
	python3 test/package_safety.py
	python3 test/delivery_safety.py
	sh test/session_edits.sh
	python3 test/entropy_analysis.py
	sh test/changelog.sh
	sh test/flags.sh "$(MAKE)"
# Everything again under AddressSanitizer + UBSan: unit tests, parser robustness, the CLI's feature tests.
check-san:
	@$(MAKE) -s TARGET=host-san san-note
	$(MAKE) TARGET=host-san build/host-san/unittest build/host-san/robust build/host-san/weather build/host-san/connect-budget-test build/host-san/ot-shutdown-test
	sh test/ot-shutdown.sh build/host-san/ot-shutdown-test
	build/host-san/unittest
	build/host-san/connect-budget-test
	build/host-san/robust
	sh test/feature.sh build/host-san/weather
	python3 test/cache_cli.py build/host-san/weather

	@if [ -f /usr/include/X11/Xlib.h ]; then \
	    $(MAKE) TARGET=host-san X11_LIBS=-lX11 build/host-san/cde-text-test build/host-san/cde-field-test && \
	    build/host-san/cde-text-test && sh test/field-host.sh build/host-san/cde-field-test; \
	else echo "check-san: CDE tests skipped (no X11 headers)"; fi
	$(MAKE) TARGET=host-san TEST_HOOKS=0 check-test-hooks

san-note:
	@echo "check-san: $(SAN_NOTE)"

check-gui: $(BUILD)/xweather $(BUILD)/xdrive $(BUILD)/xweather-search-test $(BUILD)/xweather-stale-test $(BUILD)/cde-text-test $(BUILD)/cde-field-test
	$(BUILD)/cde-text-test
	sh test/field-host.sh $(BUILD)/cde-field-test
	sh test/gui-host.sh

check-gui-maul:
	$(MAKE) TARGET=netbsd-macppc all build/netbsd-macppc/xdrive build/netbsd-macppc/xweather-search-test build/netbsd-macppc/xweather-stale-test
	sh test/gui-maul.sh

check-cross:
	@echo "TARGET=$(TARGET) cannot run here: use 'make smoke-maul' (and later 'make check-maul')"; false

check-maul:
	$(MAKE) TARGET=netbsd-macppc build/netbsd-macppc/unittest build/netbsd-macppc/robust build/netbsd-macppc/connect-budget-test
	sh test/check-maul.sh build/netbsd-macppc/unittest build/netbsd-macppc/robust build/netbsd-macppc/connect-budget-test

smoke-maul:
	$(MAKE) TARGET=netbsd-macppc all
	$(MAKE) TARGET=netbsd-macppc TEST_HOOKS=1 build/netbsd-macppc-test-hooks/weather
	sh test/smoke-maul.sh build/netbsd-macppc/weather build/netbsd-macppc-test-hooks/weather

# Layering rule: core/ and app/ may include only freestanding headers.
ALLOWED_INCLUDES = cstddef|cstdint|cstring|cstdarg|string_view|utility|type_traits|limits
lint:
	@bad=$$(grep -rn --include='*.hpp' --include='*.cpp' --include='*.h' '#include <' src/core src/app 2>/dev/null \
	    | grep -v -E '#include <($(ALLOWED_INCLUDES))>' || true); \
	if [ -n "$$bad" ]; then echo "lint: OS/hosted headers in portable layers:"; echo "$$bad"; exit 1; fi; \
	echo "lint: core/ and app/ include only freestanding headers"

clean:
	rm -rf build

-include $(wildcard $(OBJ)/*.d $(OBJ)/*/*.d $(OBJ)/*/*/*.d)

.PHONY: check-test-hooks check-ot-layout os9 FORCE san-note check-san anchors check-anchors check-tls all ppc check check-native check-gui check-gui-maul check-cross check-maul smoke-maul lint clean
