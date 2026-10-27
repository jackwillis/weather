// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#pragma once
#include "core/net.hpp"
namespace wx {
// BearSSL client: installs Net::tls_open / tls_close. One connection at a time.
// Only WEATHER_TEST_HOOKS builds accept WEATHER_NO_TA to empty the trust set.
// Production builds ignore environment clock/trust overrides.
// Compile-time WEATHER_TEST_ANCHORS makes tls.cpp include test_anchors.h instead of anchors.h (test/tls_local.sh).
void tls_net_init(Net *net);
}
