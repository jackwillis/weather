// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#pragma once
#include "core/net.hpp"
#include <cstdint>
namespace wx {
// Open Transport (Mac OS 8/9) implementation of the Net seam: connect/close/sleep/now/entropy.
void os9_net_init(Net *net);
// Exclude only completed, non-overlapping Toolbox modal tracking intervals.
void os9_credit_modal_ticks(uint32_t begin, uint32_t end);
uint32_t os9_active_ticks();
long long os9_active_milliseconds();
// Terminal shutdown: closes OT and wipes/releases the TLS seed pool. Registered
// with atexit; safe to call twice. Reinitialization after shutdown is unsupported.
void os9_net_shutdown();
// Static diagnostic message, empty until an entropy operation fails.
const char *os9_entropy_error();
}
