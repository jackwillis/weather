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
	Status (*remove)(CacheStore &) = nullptr;
};
// One storage preference applies to every provider.
inline bool cache_storage_needed(const Config &cfg)
{
	return cfg.cache;
}
inline Status remove_cached_forecast(CacheStore &store)
{
    return store.remove ? store.remove(store) : make(Err::Io,"cannot remove saved forecast");
}
inline CacheKey cache_key(const Config &cfg)
{
	return CacheKey{Query{cfg.lat_e4,cfg.lon_e4,cfg.metric},cfg.providers};
}
// One file holds the latest display forecast and, when it differs, the MET
// response. Keeping MET avoids losing its Expires/validator when OM recovers.
constexpr size_t kForecastStoreBytes=2*kForecastCacheBytes;
inline Status read_cache_records(CacheStore &store,unsigned char *record,size_t *n)
{
    Status s=store.load(store,record,kForecastStoreBytes+1,n);
    if(!s.ok() || !*n) return s;
    if(*n!=kForecastCacheBytes && *n!=kForecastStoreBytes)
        return make(Err::BadData,"invalid forecast cache length");
    return okay();
}
inline Status read_cached_forecast(CacheStore &store,const CacheKey &key,long long now,CachedForecast *out,bool *found)
{
    *found=false;
    unsigned char record[kForecastStoreBytes+1]; size_t n=0;
    Status s=read_cache_records(store,record,&n);
    if(!s.ok() || !n) return s;
    if(!cache_decode(record,kForecastCacheBytes,key,now,out)) return make(Err::BadData,"cache corrupt, expired or for different settings");
    *found=true; return okay();
}
inline Status read_cached_met(CacheStore &store,const CacheKey &key,long long now,CachedForecast *out)
{
    unsigned char record[kForecastStoreBytes+1]; size_t n=0;
    Status s=read_cache_records(store,record,&n);
    if(!s.ok()) return s;
    // No match is an ordinary cache miss; never modify the caller's value.
    for(size_t offset=0;offset<n;offset+=kForecastCacheBytes) {
        CachedForecast candidate;
        if(cache_decode(record+offset,kForecastCacheBytes,key,now,&candidate,true) && sv(candidate.forecast.source)=="met-no") {
            *out=candidate; return okay();
        }
    }
    return make(Err::NoSource,"no matching saved MET forecast");
}
inline Status write_cached_forecast(CacheStore &store,const CacheKey &key,const Forecast &forecast,long long at)
{
    unsigned char record[kForecastStoreBytes]; size_t size=kForecastCacheBytes;
    if(!cache_encode(key,forecast,at,record)) return make(Err::BadData,"forecast cannot be cached");
    if(sv(forecast.source)!="met-no" && cache_detail::selected(key.providers,"met-no") && store.load) {
        CachedForecast met;
        Status loaded=read_cached_met(store,key,at,&met);
        if(loaded.ok()) {
            if(!cache_encode(key,met.forecast,met.fetched_at,record+size)) return make(Err::BadData,"MET forecast cannot be preserved");
            size+=kForecastCacheBytes;
        } else if(loaded.err==Err::Io) return loaded; // do not erase unreadable MET metadata
    }
    return store.save(store,record,size);
}
inline Status update_forecast_cache(CacheStore &store,const CacheKey &key,const Forecast &forecast,
    long long at,bool enabled)
{
	if(!enabled || forecast.saved_valid) return okay();
	return write_cached_forecast(store,key,forecast,at);
}

// Load only a matching validated record; callers retain existing save/fallback policy.
inline Status fetch_forecast_cached(Net &net,const Provider *const *providers,size_t count,
    const Query &query,const FetchOpts &opts,char *buffer,size_t cap,Forecast *out,
    CacheStore *store,sv selection)
{
	ConditionalForecast previous; FetchOpts request=opts;
	request.conditional=nullptr;
	if(opts.save_forecasts && store && !opts.force_host && net.now) {
		CachedForecast cached;
		Status s=read_cached_met(*store,CacheKey{query,selection},net.now(net),&cached);
		if(s.ok()) {
			previous.query=query; previous.forecast=cached.forecast; previous.fetched_at=cached.fetched_at; request.conditional=&previous;
		} else if(s.err!=Err::NoSource) {
            logf(LogWarn,"%s",s.msg);
            if(s.err==Err::Io) request.save_forecasts=false;
        }
	}
	return fetch_forecast(net,providers,count,query,request,buffer,cap,out);
}

}
