// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#pragma once
#include "core/geocoding.hpp"
namespace wx {
// Only a separately linked diagnostic passes a fixture lookup. Production uses TLS.
using PlaceLookup=Status (*)(Net &,sv,const FetchOpts &,char *,size_t,PlaceResults *);
int run_xweather(int argc,char **argv,PlaceLookup lookup=nullptr);
}
