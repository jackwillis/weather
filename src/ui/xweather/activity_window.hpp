// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#pragma once
#include "app/activity.hpp"
#include "cde.hpp"

namespace cde {

bool activity_local_time(wx::ActivityTime time, char *out, size_t capacity);
struct ActivityLayout { int ascent, row_height; size_t rows; };
ActivityLayout activity_layout(int height, int text_ascent, int text_descent, int number_ascent, int number_descent);

// One ordinary window, sharing the session's immutable entry view and theme
// fonts. Only its own GC, numeric font, pixmap and window are owned here.
class ActivityWindow {
public:
    ActivityWindow() = default;
    ~ActivityWindow() { close(); }
    ActivityWindow(const ActivityWindow &) = delete;
    ActivityWindow &operator=(const ActivityWindow &) = delete;
    bool show(Theme &theme, const wx::ActivityLog &history);
    bool handle(XEvent &event);
    void changed(bool evicted = false);
    void paint();
    void close();
    Window window() const { return window_; }
private:
    Theme theme_{};
    const wx::ActivityLog *history_ = nullptr;
    Window window_ = 0;
    Pixmap backing_ = 0;
    XFontStruct *digits_ = nullptr;
    Atom protocols_ = 0, delete_ = 0;
    int width_ = 600, height_ = 300, row_height_ = 18, baseline_ascent_ = 10;
    int pressed_ = -1, focus_ = 2;
    size_t first_ = 0;
    bool follow_ = true, dirty_ = true;
    size_t page() const;
    size_t last_start() const;
    Rect button_rect(int index) const;
    void scroll(int rows);
    void activate(int index);
    void render();
};
} // namespace cde
