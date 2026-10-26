#pragma once
#include "core/settings.hpp"

namespace wx {
// Load all settings before committing any of them. A malformed later line must
// not leave half of a location or unit change applied.
inline Status load_config(SettingsStore &store, Config &cfg)
{
	char text[4096]; size_t n = 0;
	Status s = store.load(store, text, sizeof text, &n);
	if (!s.ok()) return s;
	if (n >= sizeof text) return make(Err::Config, "settings file too large");
	Config next = cfg;
	s = config_parse(sv(text, n), &next);
	if (s.ok()) cfg = next;
	return s;
}
}
