#pragma once
#include "core/settings.hpp"
namespace wx {
// Read-only app-local weather.conf. Preferences persistence is a separate slice.
SettingsStore os9_config_store(const char *path = "weather.conf");
}
