// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#pragma once
#include "core/config.hpp"
#include "core/fetch.hpp"
#include "core/log.hpp"
#include "core/forecast_cache.hpp"

namespace wx {
struct CacheStore {
	void *ctx = nullptr;
	Status (*load)(CacheStore &,unsigned char *,size_t,size_t *) = nullptr;
	Status (*save)(CacheStore &,const unsigned char *,size_t) = nullptr;
};
// MET response caching supports conditional requests independently of offline fallback.
inline bool cache_storage_needed(const Config &cfg)
{
	return cfg.cache || (cfg.contact[0] && cache_detail::selected(cfg.providers,"met-no"));
}
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
inline Status update_forecast_cache(CacheStore &store,const CacheKey &key,const Forecast &forecast,
    long long at,bool offline_fallback)
{
	if(!offline_fallback && (!forecast.source || sv(forecast.source)!="met-no")) return okay();
	return write_cached_forecast(store,key,forecast,at);
}

// Load only a matching validated record; callers retain existing save/fallback policy.
inline Status fetch_forecast_cached(Net &net,const Provider *const *providers,size_t count,
    const Query &query,const FetchOpts &opts,char *buffer,size_t cap,Forecast *out,
    CacheStore *store,sv selection)
{
	ConditionalForecast previous; FetchOpts request=opts;
	request.conditional=nullptr;
	if(store && !opts.force_host && net.now) {
		CachedForecast cached; bool found=false;
		Status s=read_cached_forecast(*store,CacheKey{query,selection},net.now(net),&cached,&found);
		if(s.ok() && found) {
			previous.query=query; previous.forecast=cached.forecast; request.conditional=&previous;
		} else if(!s.ok()) logf(LogWarn,"%s",s.msg);
	}
	return fetch_forecast(net,providers,count,query,request,buffer,cap,out);
}

}
