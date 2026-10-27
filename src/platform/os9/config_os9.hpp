// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#pragma once
#include "core/settings.hpp"
namespace wx {
// Read-only legacy app-local weather.conf, used until nonempty Preferences exist.
SettingsStore os9_config_store(const char *path = "weather.conf");
}
