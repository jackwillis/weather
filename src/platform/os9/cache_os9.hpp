// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#pragma once
#include "app/cache_store.hpp"
#include <Files.h>

namespace wx {
struct Os9CacheFileOps {
	OSErr (*read)(short,long *,void *);
	OSErr (*write)(short,long *,const void *);
	OSErr (*close)(short);
	OSErr (*flush)(short);
	OSErr (*exchange)(FSSpec *,FSSpec *);
	OSErr (*rename)(FSSpec *,const unsigned char *);
	OSErr (*remove)(FSSpec *);
};
const Os9CacheFileOps &os9_cache_file_ops();
struct Os9Cache {
	CacheStore base;
	FSSpec file = {};
	const Os9CacheFileOps *ops = nullptr;
};
// Default: Preferences:Weather Forecast Cache. The optional diagnostic path is
// supplied only by the separately linked probe, never read from a marker/config.
Status os9_cache_init(Os9Cache *,const char *diagnostic_path = nullptr,const Os9CacheFileOps *ops = nullptr);
}
