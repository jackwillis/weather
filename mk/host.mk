# Native build on the development machine (Linux). Runs the tests directly.
CXX         = c++
STRIP       = cp $< $@
BEARSSL_BUILD    = buildhost
BEARSSL_MAKEVARS =
EXTRA_CXXFLAGS = -Werror
EXTRA_LDFLAGS  =
CHECK_TARGET   = check-native
