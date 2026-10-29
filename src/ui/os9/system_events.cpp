// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#include "system_events.hpp"
namespace wx {
void (*SystemEvents::quit_)() = nullptr;
}
