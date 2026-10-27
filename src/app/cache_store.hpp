// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#pragma once
#include "core/config.hpp"
#include "core/forecast_cache.hpp"

namespace wx {
struct CacheStore {
	void *ctx = nullptr;
	Status (*load)(CacheStore &,unsigned char *,size_t,size_t *) = nullptr;
	Status (*save)(CacheStore &,const unsigned char *,size_t) = nullptr;
};
inline CacheKey cache_key(const Config &cfg)
{
	return CacheKey{Query{cfg.lat_e4,cfg.lon_e4,cfg.metric},cfg.providers};
}
inline Status read_cached_forecast(CacheStore &store,const CacheKey &key,long long now,CachedForecast *out,bool *found)
{
	*found=false;
	unsigned char record[kForecastCacheBytes+1]; size_t n=0;
	Status s=store.load(store,record,sizeof record,&n);
	if(!s.ok() || !n) return s;
	if(n>kForecastCacheBytes || !cache_decode(record,n,key,now,out)) return make(Err::BadData,"cache corrupt, expired or for different settings");
	*found=true; return okay();
}
inline Status write_cached_forecast(CacheStore &store,const CacheKey &key,const Forecast &forecast,long long at)
{
	unsigned char record[kForecastCacheBytes];
	if(!cache_encode(key,forecast,at,record)) return make(Err::BadData,"forecast cannot be cached");
	return store.save(store,record,sizeof record);
}
}
