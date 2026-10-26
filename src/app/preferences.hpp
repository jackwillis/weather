// OS-independent Preferences policy: full source precedence and save-before-apply.
#pragma once
#include "locform.hpp"
#include "model.hpp"
#include "core/settings.hpp"

namespace wx {
inline Status preference_text(SettingsStore &prefs, SettingsStore &legacy, char *text, size_t cap, size_t *n)
{
    if (!prefs.load || !legacy.load) return make(Err::Config, "settings store unavailable");
    Status s = prefs.load(prefs, text, cap, n);
    if (s.ok() && *n == 0) s = legacy.load(legacy, text, cap, n);
    if (s.ok() && *n >= cap) return make(Err::Config, "settings file too large");
    return s;
}
inline Status load_preferences(SettingsStore &prefs, SettingsStore &legacy, Config &cfg)
{
    char text[4096]; size_t n = 0;
    Status s = preference_text(prefs, legacy, text, sizeof text, &n);
    Config next; // Preferences is the whole configuration, not an overlay on a prior load.
    if (s.ok()) s = config_parse(sv(text, n), &next);
    if (s.ok()) cfg = next;
    return s;
}
inline Status commit_preferences(SettingsStore &prefs, SettingsStore &legacy, const Config &candidate,
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
// After repairing a failed startup load, honor the entire retained configuration
// before allowing a fetch; ordinary UI saves do not silently reload other keys.
inline Status recover_preferences(SettingsStore &prefs, SettingsStore &legacy, Config &cfg, Model &model, long long now)
{
    if (model.phase() == Phase::Fetching) return make(Err::Config,"cannot reload settings during an update");
    Status s=load_preferences(prefs,legacy,cfg);
    if (s.ok()) { model.set_auto_refresh(cfg.auto_refresh,now); model.location_changed(); }
    return s;
}
struct LocationEditor {
    LocationForm form;
    bool visible = false, auto_refresh = true, committed = false;
    Status error;
    bool open(const Config &cfg, const Model &model) {
        if (visible || model.phase() == Phase::Fetching) return false;
        form.load(cfg); auto_refresh=cfg.auto_refresh; visible=true; committed=false; error=okay(); return true;
    }
    void cancel() { visible=false; committed=false; error=okay(); }
    Status apply(SettingsStore &prefs, SettingsStore &legacy, Config &cfg, Model &model, long long now) {
        if (!visible) return make(Err::Config,"location editor is closed");
        Config next=cfg;
        error=form.read(&next);
        if (error.ok()) { next.auto_refresh=auto_refresh; error=commit_preferences(prefs,legacy,next,cfg,model,now); }
        if (error.ok()) { visible=false; committed=true; }
        return error;
    }
};
}
