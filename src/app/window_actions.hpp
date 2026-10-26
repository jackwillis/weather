// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Command policy shared by the native window and its diagnostic driver.
// During a yielding fetch, only Quit and the informational About overlay change.
#pragma once
#include "core/config.hpp"
#include "model.hpp"

namespace wx {
enum class WindowAction { Refresh, Quit, ToggleAuto, Metric, Imperial, About, DismissAbout, Location };

inline bool window_seed_failure(const Status &status, sv seed_reason)
{
	return !seed_reason.empty() && status.err == Err::Tls && status.msg && sv(status.msg) == "no source of random numbers";
}

struct WindowActions {
	bool quit = false;
	bool about = false;

	bool apply(WindowAction action, Config &cfg, Model &model, long long now)
	{
		if (quit) return false;
		if (action == WindowAction::Quit) { quit = true; return true; }
		if (action == WindowAction::About) { bool changed = !about; about = true; return changed; }
		if (action == WindowAction::DismissAbout) { bool changed = about; about = false; return changed; }
		if (model.phase() == Phase::Fetching) return false;
		switch (action) {
		case WindowAction::Location: return false; // The native dialog owns its save transaction.
		case WindowAction::Refresh: model.request_refresh(); break;
		case WindowAction::ToggleAuto:
			cfg.auto_refresh = !cfg.auto_refresh;
			model.set_auto_refresh(cfg.auto_refresh, now);
			break;
		case WindowAction::Metric:
		case WindowAction::Imperial: {
			bool metric = action == WindowAction::Metric;
			if (metric != cfg.metric) {
				cfg.metric = metric;
				// Old values have different units; do not relabel them while fetching.
				model.location_changed();
			} else return false;
			break;
		}
		case WindowAction::About:
		case WindowAction::DismissAbout:
		case WindowAction::Quit: break;
		}
		return true;
	}
};
} // namespace wx
