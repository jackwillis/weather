// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
struct PrefMemory {
    SettingsStore base;
    char text[4096] = {};
    bool fail_load = false, fail_save = false;
    int reads = 0, writes = 0;
    PrefMemory() {
        base.ctx = this;
        base.load = [](SettingsStore &s, char *out, size_t cap, size_t *n) {
            auto &m = *static_cast<PrefMemory *>(s.ctx); ++m.reads;
            if (m.fail_load) return make(Err::Io, "fixture read failure");
            *n = strlen(m.text);
            if (*n >= cap) return make(Err::Config, "fixture too large");
            memcpy(out, m.text, *n); return okay();
        };
        base.save = [](SettingsStore &s, const char *p, size_t n) {
            auto &m = *static_cast<PrefMemory *>(s.ctx); ++m.writes;
            if (m.fail_save) return make(Err::Io, "fixture save failure");
            if (n >= sizeof m.text) return make(Err::Config, "fixture too large");
            memcpy(m.text, p, n); m.text[n] = 0; return okay();
        };
    }
};
static void test_preferences()
{
    PrefMemory prefs, legacy;
    strcpy(legacy.text, "# legacy\nlatitude=1 # home\nlongitude=2\ncontact=https://example.invalid/test\nproviders=met-no\n");
    Config cfg;
    CHECK(load_preferences(prefs.base, legacy.base, cfg).ok() && cfg.lat_e4 == 10000);
    CHECK(!strcmp(cfg.providers, "met-no") && prefs.writes == 0);
    strcpy(prefs.text, "latitude=3\n");
    CHECK(load_preferences(prefs.base, legacy.base, cfg).ok() && cfg.lat_e4 == 30000);
    CHECK(!strcmp(cfg.providers, "open-meteo")); // Preferences is the complete source.
    strcpy(prefs.text, "latitude=4\nunits=bad\n");
    CHECK(!load_preferences(prefs.base, legacy.base, cfg).ok() && cfg.lat_e4 == 30000);
    prefs.fail_load = true;
    CHECK(!load_preferences(prefs.base, legacy.base, cfg).ok());
    prefs.fail_load = false; prefs.text[0] = 0;
    CHECK(load_preferences(prefs.base, legacy.base, cfg).ok());
    Model model;
    CHECK(model.tick(0)); Forecast f = some_forecast(550); model.finish(1, okay(), &f);
    LocationEditor editor;
    CHECK(editor.open(cfg, model));
    editor.form.lat.set("bad");
    CHECK(!editor.apply(prefs.base, legacy.base, cfg, model, 2).ok() && editor.visible);
    CHECK(prefs.writes == 0 && cfg.lat_e4 == 10000 && model.has_forecast());
    editor.form.lat.set("91");
    CHECK(!editor.apply(prefs.base, legacy.base, cfg, model, 2).ok() && editor.form.focus == LocationForm::Lat);
    editor.form.lat.set("10"); editor.form.lon.set("181");
    CHECK(!editor.apply(prefs.base, legacy.base, cfg, model, 2).ok() && editor.form.focus == LocationForm::Lon);
    editor.form.lon.set("20"); editor.form.name.set("bad#name");
    CHECK(!editor.apply(prefs.base, legacy.base, cfg, model, 2).ok() && editor.form.focus == LocationForm::Name);
    editor.form.name.set("New place"); editor.form.metric = true; editor.auto_refresh = false;
    prefs.fail_save = true;
    CHECK(!editor.apply(prefs.base, legacy.base, cfg, model, 2).ok() && editor.visible);
    CHECK(cfg.lat_e4 == 10000 && !cfg.metric && cfg.auto_refresh && model.has_forecast());
    prefs.fail_save = false;
    CHECK(editor.apply(prefs.base, legacy.base, cfg, model, 3).ok() && !editor.visible);
    CHECK(editor.committed);
    CHECK(cfg.lat_e4 == 100000 && cfg.lon_e4 == 200000 && cfg.metric && !cfg.auto_refresh && !model.has_forecast());
    CHECK(strstr(prefs.text, "# legacy") && strstr(prefs.text, "# home") && strstr(prefs.text, "providers=met-no"));
    CHECK(strstr(prefs.text, "contact=https://example.invalid/test") && strstr(prefs.text, "autorefresh = off"));
    CHECK(strstr(legacy.text, "latitude=1") != nullptr);
    CHECK(model.tick(3) && !model.tick(4));
    int writes = prefs.writes;
    CHECK(!editor.open(cfg, model));
    CHECK(!commit_preferences(prefs.base, legacy.base, cfg, cfg, model, 4).ok() && prefs.writes == writes);
    model.finish(4, okay(), &f);
    CHECK(editor.open(cfg, model)); editor.form.lat.set("80"); editor.cancel();
    CHECK(!editor.visible && !editor.committed && cfg.lat_e4 == 100000 && prefs.writes == writes && model.has_forecast());
    Config restart;
    CHECK(load_preferences(prefs.base, legacy.base, restart).ok() && restart.lat_e4 == cfg.lat_e4 && restart.metric && !restart.auto_refresh);
    Config saving=cfg; saving.cache=false;
    CHECK(commit_preferences(prefs.base,legacy.base,saving,cfg,model,5).ok());
    CHECK(load_preferences(prefs.base,legacy.base,restart).ok() && !restart.cache);
    CHECK(model.has_forecast()); // Preference does not erase the current in-memory forecast.
    Config renamed = cfg; strcpy(renamed.name, "Renamed");
    CHECK(commit_preferences(prefs.base, legacy.base, renamed, cfg, model, 5).ok() && model.has_forecast() && !model.tick(5));
    // A newly edited unrelated setting/comment is retained on save (no stale text snapshot).
    strcat(prefs.text, "# edited externally\ntimeout=27\n");
    CHECK(commit_preferences(prefs.base, legacy.base, cfg, cfg, model, 6).ok());
    CHECK(strstr(prefs.text, "# edited externally") && strstr(prefs.text, "timeout=27"));
    prefs.fail_load = true; writes = prefs.writes;
    CHECK(!commit_preferences(prefs.base, legacy.base, cfg, cfg, model, 7).ok() && prefs.writes == writes);
    // Repairing a startup error must honor retained provider settings before a fetch.
    prefs.fail_load=false;
    strcpy(prefs.text,"providers=met-no\nlatitude=oops\n");
    Config broken;
    CHECK(!load_preferences(prefs.base,legacy.base,broken).ok());
    Model repaired_model; LocationEditor repair;
    CHECK(repair.open(broken,repaired_model)); repair.form.lat.set("10");
    CHECK(repair.apply(prefs.base,legacy.base,broken,repaired_model,10).ok());
    CHECK(recover_preferences(prefs.base,legacy.base,broken,repaired_model,10).ok());
    CHECK(!strcmp(broken.providers,"met-no") && broken.lat_e4==100000);
    CHECK(repaired_model.tick(10) && !repaired_model.tick(11));
}
