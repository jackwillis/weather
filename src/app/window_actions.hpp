// Command policy shared by the native window and its diagnostic driver.
// During a yielding fetch, only Quit may change application state.
#pragma once
#include "core/config.hpp"
#include "model.hpp"

namespace wx {
enum class WindowAction { Refresh, Quit, ToggleAuto, Metric, Imperial, About, DismissAbout };

struct WindowActions {
	bool quit = false;
	bool about = false;

	void apply(WindowAction action, Config &cfg, Model &model, long long now)
	{
		if (quit) return;
		if (action == WindowAction::Quit) { quit = true; return; }
		if (model.phase() == Phase::Fetching) return;
		switch (action) {
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
			}
			break;
		}
		case WindowAction::About: about = true; break;
		case WindowAction::DismissAbout: about = false; break;
		case WindowAction::Quit: break;
		}
	}
};
} // namespace wx
