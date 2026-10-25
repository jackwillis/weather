#pragma once
#include "core/net.hpp"
namespace wx {
// BearSSL client: installs Net::tls_open / tls_close. One connection at a time.
// Environment WEATHER_NO_TA=1 empties the trust set (test hook: must fail verification).
// Compile-time WEATHER_TEST_ANCHORS makes tls.cpp include test_anchors.h instead of anchors.h (test/tls_local.sh).
void tls_net_init(Net *net);
}
