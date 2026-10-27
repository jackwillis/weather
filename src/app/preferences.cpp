// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Keep mutable workspace out of inline-function weak storage: Retro68 may
// overlap that storage with other globals. This single definition owns both buffers.
#include "preferences.hpp"

namespace wx {
Status commit_preferences(SettingsStore &prefs, SettingsStore &legacy, const Config &candidate,
                                 Config &cfg, Model &model, long long now)
{
    if (model.phase() == Phase::Fetching) return make(Err::Config, "wait for the current update before editing settings");
    if (!prefs.save) return make(Err::Config, "preferences cannot be saved");
    static char before[4096], after[4096]; // Single-threaded UI, bounded and no heap.
    size_t n = 0, written = 0;
    Status s = preference_text(prefs, legacy, before, sizeof before, &n);
    if (!s.ok()) return s;
    Buf<16> lat, lon;
    lat.add_fixed(candidate.lat_e4, 4); lon.add_fixed(candidate.lon_e4, 4);
    KeyVal kv[] = {{"latitude",lat.view()}, {"longitude",lon.view()}, {"name",candidate.name},
                   {"units",candidate.metric?"metric":"imperial"}, {"autorefresh",candidate.auto_refresh?"on":"off"}};
    s = settings_update(sv(before,n),kv,5,after,sizeof after,&written);
    if (s.ok()) s = prefs.save(prefs,after,written);
    if (!s.ok()) return s;
    bool changed = cfg.lat_e4 != candidate.lat_e4 || cfg.lon_e4 != candidate.lon_e4 || cfg.metric != candidate.metric;
    bool auto_changed = cfg.auto_refresh != candidate.auto_refresh;
    cfg = candidate;
    if (auto_changed) model.set_auto_refresh(cfg.auto_refresh,now);
    if (changed) model.location_changed();
    return okay();
}
} // namespace wx
