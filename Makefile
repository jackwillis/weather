# GNU make required (BSD hosts: gmake). One fragment per target in mk/.
#
#   make                      build for this machine           -> build/host/
#   make TARGET=netbsd-macppc cross-build for NetBSD/macppc    -> build/netbsd-macppc/
#   make check                unit + feature tests (host), see mk/host.mk
#   make lint                 portable layers must not include OS headers
#   make check-maul           cross-build the unit tests and run them on maul (big-endian ground truth)
#   make smoke-maul           cross-build, push to maul, test the real binary there
#   make clean
# Shortcut: `make ppc` = make TARGET=netbsd-macppc.

TARGET  ?= host
VERSION  = 0.1.0
BEARSSL ?= spike/bearssl

include mk/$(TARGET).mk

BUILD = build/$(TARGET)
OBJ   = $(BUILD)/obj
WARN  = -Wall -Wextra -Wno-unused-parameter
CXXFLAGS_ALL = -std=c++17 -O2 -g $(WARN) $(EXTRA_CXXFLAGS) -fno-exceptions -fno-rtti \
    -MMD -MP -DWEATHER_VERSION='"$(VERSION)"' -Isrc -I$(BEARSSL)/inc $(CXXFLAGS)

BEARSSL_LIB = $(BEARSSL)/$(BEARSSL_BUILD)/libbearssl.a

# Layers (see docs/ARCHITECTURE.md). Platform is chosen by the target; only posix exists so far.
PLATFORM     = posix
CORE_SRCS    = src/core/fetch.cpp
TLS_SRCS     = src/tls/tls.cpp
PLAT_SRCS    = src/platform/$(PLATFORM)/net_posix.cpp src/platform/$(PLATFORM)/log_posix.cpp \
               src/platform/$(PLATFORM)/settings_posix.cpp src/platform/$(PLATFORM)/nocxx.cpp
CLI_SRCS     = src/cli/main.cpp
WEATHER_OBJS = $(patsubst src/%.cpp,$(OBJ)/%.o,$(CORE_SRCS) $(TLS_SRCS) $(PLAT_SRCS) $(CLI_SRCS))
TEST_OBJS    = $(OBJ)/test/unittest.o $(OBJ)/core/fetch.o $(OBJ)/platform/$(PLATFORM)/log_posix.o \
               $(OBJ)/platform/$(PLATFORM)/net_posix.o $(OBJ)/platform/$(PLATFORM)/settings_posix.o

all: $(BUILD)/weather
ppc:
	$(MAKE) TARGET=netbsd-macppc all

$(BUILD)/weather.debug: $(WEATHER_OBJS) $(BEARSSL_LIB)
	$(CXX) $(EXTRA_LDFLAGS) -o $@ $(WEATHER_OBJS) $(BEARSSL_LIB)

$(BUILD)/weather: $(BUILD)/weather.debug
	$(STRIP)

$(BUILD)/unittest: $(TEST_OBJS)
	$(CXX) $(EXTRA_LDFLAGS) -o $@ $(TEST_OBJS)

$(OBJ)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_ALL) -c -o $@ $<

$(OBJ)/test/unittest.o: test/unittest.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_ALL) -DFIXTURE_DIR='"test/fixtures"' -c -o $@ $<

$(BEARSSL_LIB):
	$(MAKE) -C $(BEARSSL) BUILD=$(BEARSSL_BUILD) CONF=Unix $(BEARSSL_MAKEVARS) lib

# ---- tests
check: $(CHECK_TARGET)
check-native: lint $(BUILD)/unittest $(BUILD)/weather
	$(BUILD)/unittest
	sh test/feature.sh $(BUILD)/weather
check-cross:
	@echo "TARGET=$(TARGET) cannot run here: use 'make smoke-maul' (and later 'make check-maul')"; false

check-maul:
	$(MAKE) TARGET=netbsd-macppc build/netbsd-macppc/unittest
	sh test/check-maul.sh build/netbsd-macppc/unittest

smoke-maul:
	$(MAKE) TARGET=netbsd-macppc all
	sh test/smoke-maul.sh build/netbsd-macppc/weather

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

.PHONY: all ppc check check-native check-cross check-maul smoke-maul lint clean
