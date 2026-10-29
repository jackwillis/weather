// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#pragma once
#include "app/window_actions.hpp"
#include "app/cache_store.hpp"
#include "core/net.hpp"
#include "core/settings.hpp"
#include "core/geocoding.hpp"
namespace wx {
// The diagnostic executable supplies these callbacks. Production passes nullptr
// and does not link any marker-file reader or fixture generator.
struct WindowTestHooks {
	void (*setup)(Config &) = nullptr;
	void (*idle)() = nullptr;
	Status (*fetch)(const Config &, Net &, Forecast *) = nullptr;
	SettingsStore *preferences = nullptr;
	SettingsStore *legacy = nullptr;
	bool first_run = false; // Explicit diagnostic scenario; production detects empty settings.
	Status (*search)(Net &,sv,PlaceResults *) = nullptr;
	CacheStore *cache_store = nullptr;
};
int run_os9_window(const WindowTestHooks *hooks = nullptr);
void os9_window_action(WindowAction action);
void os9_window_key(char key, bool command_key);
const Model &os9_window_model();
void os9_window_invalidate();
bool os9_window_location_visible();
void os9_window_location_field(int,sv);
void os9_window_location_item(int);
int os9_window_location_focus();
bool os9_window_place_visible();
void os9_window_place_query(sv);
void os9_window_place_item(int);
void os9_window_place_key(char,bool command=false);
size_t os9_window_place_count();
int os9_window_place_selected();
void os9_window_location_key(char,bool shift=false);
const Config &os9_window_config();
Status os9_window_settings_status();
unsigned long os9_window_updates();
bool os9_window_buffered();
void os9_window_menu_selection(long selection);
unsigned long os9_window_suspends();
unsigned long os9_window_resumes();
bool os9_window_foreground();
Status os9_window_fetch_live(const Config &, Net &, Forecast *);
}
