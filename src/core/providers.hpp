// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Every data source we know, in default order. The CLI and the windows both resolve names against this.
#pragma once
#include "metno.hpp"
#include "openmeteo.hpp"
#include "nws.hpp"

namespace wx {

inline constexpr const Provider *kProviders[] = { &kOpenMeteo, &kMetNo, &kNws };
inline constexpr size_t kNumProviders = sizeof kProviders / sizeof kProviders[0];

} // namespace wx
