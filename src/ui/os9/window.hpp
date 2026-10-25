#pragma once
#include "app/window_actions.hpp"
#include "core/net.hpp"
namespace wx {
// The diagnostic executable supplies these callbacks. Production passes nullptr
// and does not link any marker-file reader or fixture generator.
struct WindowTestHooks {
	void (*setup)(Config &) = nullptr;
	void (*idle)() = nullptr;
	Status (*fetch)(const Config &, Net &, Forecast *) = nullptr;
};
int run_os9_window(const WindowTestHooks *hooks = nullptr);
void os9_window_action(WindowAction action);
const Model &os9_window_model();
void os9_window_invalidate();
unsigned long os9_window_updates();
bool os9_window_buffered();
Status os9_window_fetch_live(const Config &, Net &, Forecast *);
}
