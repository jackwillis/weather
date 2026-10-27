// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#pragma once
#include "core/net.hpp"
namespace wx {
// BSD sockets implementation of the Net seam (connect/close/sleep/now).
void posix_net_init(Net *net);
}
