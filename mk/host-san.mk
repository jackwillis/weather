# Host build with checks that abort on bugs, for `make check-san`.
# Always: UBSan in trap mode (needs no runtime library), libstdc++ assertions, fortified libc calls.
# Plus AddressSanitizer when its runtime is installed (Fedora: sudo dnf install libasan libubsan).
CXX         = c++
STRIP       = cp $< $@
BEARSSL_MAKEVARS =
EXTRA_CXXFLAGS = -Werror -fsanitize=undefined -fsanitize-undefined-trap-on-error -D_GLIBCXX_ASSERTIONS \
                 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fno-omit-frame-pointer
EXTRA_LDFLAGS  =
# Ask the compiler: Fedora and Debian/Ubuntu put this library in different places.
ASAN_RUNTIME := $(shell $(CXX) -print-file-name=libasan.so)
ifneq ($(wildcard $(ASAN_RUNTIME)),)
EXTRA_CXXFLAGS += -fsanitize=address
EXTRA_LDFLAGS  += -fsanitize=address
SAN_NOTE = with AddressSanitizer
else
ifeq ($(REQUIRE_ASAN),1)
$(error AddressSanitizer is required but its runtime was not found by $(CXX))
endif
SAN_NOTE = UBSan (trap mode) only: install libasan for AddressSanitizer
endif
CHECK_TARGET   = check-native
