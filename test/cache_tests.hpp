// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#include "core/forecast_cache.hpp"
#include "app/model.hpp"
#include "platform/posix/cache_posix.hpp"
#include <cerrno>
#include <fcntl.h>

namespace cache_test {
enum Fault { None, Read, PartialThenFail, Partial, SyncFile, CloseFile, Replace, SyncDirectory, CloseDirectory };
static Fault fault=None;
static int writes=0,syncs=0,closes=0;
static ssize_t read(int fd,void *p,size_t n) { if(fault==Read) { errno=EIO; return -1; } return ::read(fd,p,n); }
static ssize_t write(int fd,const void *p,size_t n)
{
	if(fault==PartialThenFail && writes++) { errno=EIO; return -1; }
	if(fault==PartialThenFail || fault==Partial) n=n>1?n/2:1;
	return ::write(fd,p,n);
}
static int sync(int fd)
{
	++syncs;
	if((fault==SyncFile && syncs==1) || (fault==SyncDirectory && syncs==2)) { errno=EIO; return -1; }
	return ::fsync(fd);
}
static int close(int fd)
{
	int result=::close(fd); ++closes;
	if((fault==CloseFile && closes==1) || (fault==CloseDirectory && closes==2)) { errno=EIO; return -1; }
	return result;
}
static int replace(const char *a,const char *b) { if(fault==Replace) { errno=EIO; return -1; } return ::rename(a,b); }
}

static void test_cache_storage()
{
	char dir[]="/tmp/weather-cache-XXXXXX"; CHECK(mkdtemp(dir)!=nullptr);
	char path[256]; snprintf(path,sizeof path,"%s/forecast",dir);
	PosixCache store; CHECK(posix_cache_init(&store,path).ok());
	CacheKey key{Query{},"open-meteo"};
	CachedForecast cached; bool found=true;
	constexpr long long at=1792950000LL;
	CHECK(read_cached_forecast(store.base,key,at,&cached,&found).ok() && !found);
	Forecast forecast; forecast.source="open-meteo"; forecast.temp_t10=650; forecast.code=0;
	CHECK(write_cached_forecast(store.base,key,forecast,at).ok());
	CHECK(read_cached_forecast(store.base,key,at,&cached,&found).ok() && found && cached.forecast.temp_t10==650);
	struct stat info; CHECK(stat(path,&info)==0 && (info.st_mode&0777)==0600);
	CacheFileOps ops{cache_test::read,cache_test::write,cache_test::sync,cache_test::close,cache_test::replace};
	for(cache_test::Fault fault:{cache_test::PartialThenFail,cache_test::SyncFile,cache_test::CloseFile,cache_test::Replace,cache_test::SyncDirectory,cache_test::CloseDirectory}) {
		CHECK(posix_cache_init(&store,path).ok()); forecast.temp_t10=650;
		CHECK(write_cached_forecast(store.base,key,forecast,at).ok());
		cache_test::fault=fault; cache_test::writes=cache_test::syncs=cache_test::closes=0;
		CHECK(posix_cache_init(&store,path,&ops).ok()); forecast.temp_t10=600;
		CHECK(!write_cached_forecast(store.base,key,forecast,at+1).ok());
		CHECK(posix_cache_init(&store,path).ok());
		CHECK(read_cached_forecast(store.base,key,at+1,&cached,&found).ok() && found);
		CHECK(cached.forecast.temp_t10==(fault==cache_test::SyncDirectory || fault==cache_test::CloseDirectory?600:650));
	}
	cache_test::fault=cache_test::Partial; cache_test::writes=cache_test::syncs=cache_test::closes=0;
	CHECK(posix_cache_init(&store,path,&ops).ok());
	CHECK(write_cached_forecast(store.base,key,forecast,at).ok()); // Short writes are completed.
	for(cache_test::Fault fault:{cache_test::Read,cache_test::CloseFile}) {
		cache_test::fault=fault; cache_test::closes=0;
		CHECK(!read_cached_forecast(store.base,key,at,&cached,&found).ok() && !found);
	}
	CHECK(posix_cache_init(&store,path).ok());
	int fd=open(path,O_WRONLY|O_TRUNC); CHECK(fd>=0);
	CHECK(::write(fd,"x",1)==1); CHECK(::close(fd)==0);
	CHECK(!read_cached_forecast(store.base,key,at,&cached,&found).ok() && !found);
	CHECK(unlink(path)==0);
	CHECK(symlink("missing-target",path)==0);
	CHECK(!read_cached_forecast(store.base,key,at,&cached,&found).ok() && !found);
	CHECK(unlink(path)==0);
	DIR *directory=opendir(dir); CHECK(directory!=nullptr);
	int entries=0; if(directory) { while(dirent *entry=readdir(directory)) if(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) ++entries; closedir(directory); }
	CHECK(entries==0); // Failure paths did not leak temporary files.
	CHECK(rmdir(dir)==0);
}

static void test_cache_codec()
{
	CacheKey key{Query{},"open-meteo"};
	Forecast forecast;
	forecast.source="open-meteo"; forecast.temp_t10=650; forecast.code=0;
	forecast.humidity=67; forecast.wind_t10=80; forecast.wind_dir=225; forecast.is_day=1;
	strcpy(forecast.time,"2026-10-25T12:00");
	constexpr long long at=1792950000LL;
	unsigned char record[kForecastCacheBytes];
	CHECK(cache_encode(key,forecast,at,record));
	CachedForecast decoded;
	CHECK(cache_decode(record,sizeof record,key,at,&decoded));
	CHECK(decoded.fetched_at==at && decoded.forecast.temp_t10==650);
	CHECK(!strcmp(decoded.forecast.source,"open-meteo") && !strcmp(decoded.forecast.time,forecast.time));
	CHECK(cache_decode(record,sizeof record,key,at+86400,&decoded));
	CHECK(!cache_decode(record,sizeof record,key,at+86401,&decoded));
	CHECK(!cache_decode(record,sizeof record,key,at-1,&decoded));
	CHECK(!cache_decode(record,sizeof record,key,-1,&decoded));
	CHECK(!cache_decode(record,sizeof record,key,9223372036854775807LL,&decoded));
	CacheKey wrong=key; wrong.query.lat_e4++;
	CHECK(!cache_decode(record,sizeof record,wrong,at,&decoded));
	wrong=key; wrong.query.lon_e4++;
	CHECK(!cache_decode(record,sizeof record,wrong,at,&decoded));
	wrong=key; wrong.query.metric=true;
	CHECK(!cache_decode(record,sizeof record,wrong,at,&decoded));
	wrong=key; wrong.providers="met-no";
	CHECK(!cache_decode(record,sizeof record,wrong,at,&decoded));
	for(size_t n=0;n<sizeof record;n++) CHECK(!cache_decode(record,n,key,at,&decoded));
	CHECK(!cache_decode(record,sizeof record+1,key,at,&decoded));
	for(size_t i=0;i<sizeof record;i++) {
		record[i]^=1; CHECK(!cache_decode(record,sizeof record,key,at,&decoded)); record[i]^=1;
	}
	Forecast bad=forecast; bad.temp_t10=999999;
	CHECK(!cache_encode(key,bad,at,record));
	bad=forecast; bad.humidity=101; CHECK(!cache_encode(key,bad,at,record));
	bad=forecast; bad.wind_t10=-1; CHECK(!cache_encode(key,bad,at,record));
	bad=forecast; bad.wind_dir=361; CHECK(!cache_encode(key,bad,at,record));
	bad=forecast; bad.is_day=2; CHECK(!cache_encode(key,bad,at,record));
	bad=forecast; bad.source="unknown"; CHECK(!cache_encode(key,bad,at,record));
	bad=forecast; bad.code=4; CHECK(!cache_encode(key,bad,at,record));
	bad=forecast; memset(bad.time,'x',sizeof bad.time); CHECK(!cache_encode(key,bad,at,record));
	CHECK(!cache_encode(key,forecast,-1,record));
	wrong=key; wrong.query.lat_e4=900001; CHECK(!cache_encode(wrong,forecast,at,record));
	wrong=key; wrong.providers="met-no"; CHECK(!cache_encode(wrong,forecast,at,record));
	key.providers="open-meteo, met-no"; forecast.source="met-no";
	forecast.temp_t10=-1300; forecast.humidity=kUnset; forecast.wind_t10=kUnset;
	forecast.wind_dir=kUnset; forecast.is_day=kUnset;
	CHECK(cache_encode(key,forecast,0,record));
	CHECK(cache_decode(record,sizeof record,key,0,&decoded));
	CHECK(decoded.forecast.temp_t10==-1300 && decoded.forecast.is_day==kUnset);
	key.query.metric=true; forecast.metric=true; forecast.temp_t10=-900;
	CHECK(cache_encode(key,forecast,at,record));
	CHECK(cache_decode(record,sizeof record,key,at,&decoded));
	CHECK(decoded.forecast.metric && decoded.forecast.temp_t10==-900);
	// NWS explicitly represents unavailable conditions as -1; other providers do not.
	key.providers="nws"; forecast.source="nws"; forecast.code=-1;
	CHECK(cache_encode(key,forecast,at,record));
	CHECK(record[17]==3);
	CHECK(cache_decode(record,sizeof record,key,at,&decoded));
	CHECK(!strcmp(decoded.forecast.source,"nws") && decoded.forecast.code==-1);
	forecast.code=0; CHECK(cache_encode(key,forecast,at,record));
	CHECK(cache_decode(record,sizeof record,key,at,&decoded));
	forecast.code=-2; CHECK(!cache_encode(key,forecast,at,record));
	forecast.code=4; CHECK(!cache_encode(key,forecast,at,record));
	forecast.code=-1; forecast.source="met-no"; key.providers="met-no";
	CHECK(!cache_encode(key,forecast,at,record));
	forecast.source="open-meteo"; key.providers="open-meteo";
	CHECK(!cache_encode(key,forecast,at,record));
	Config cfg; CHECK(!cfg.cache);
	CHECK(config_parse("cache=on",&cfg).ok() && cfg.cache);
	CHECK(config_parse("cache=off",&cfg).ok() && !cfg.cache);
	CHECK(!config_parse("cache=maybe",&cfg).ok());
}

static void test_cache_model()
{
	Forecast f; f.source="open-meteo"; f.temp_t10=650; f.code=0;
	Model model;
	CHECK(model.restore_cached(f,100,200));
	CHECK(model.has_forecast() && model.cached() && model.stale(200));
	Buf<256> status; model.status_line(status,200);
	CHECK(find(status.view(),"Cached / stale")==0);
	CHECK(model.tick(200));
	model.finish(201,make(Err::Timeout,"offline"),nullptr);
	CHECK(model.has_forecast() && model.cached() && model.forecast().temp_t10==650);
	model.request_refresh(); CHECK(!model.tick(202));
	CHECK(model.cached() && model.stale(202));
	CHECK(!model.tick(205)); // A rejected manual request is never queued.
	model.request_refresh(); CHECK(model.tick(205));
	model.finish(206,okay(),&f);
	CHECK(!model.cached() && !model.stale(206));
	CHECK(!model.restore_cached(f,100,206)); // Do not replace current in-memory data.
	Buf<256> live_rollback; model.status_line(live_rollback,100);
	CHECK(model.stale(100) && find(live_rollback.view(),"clock changed")!=sv::npos);
	model.location_changed(); CHECK(!model.has_forecast() && !model.cached());
	CHECK(!model.restore_cached(f,300,200));
	CHECK(!model.restore_cached(f,0,86401));
	CHECK(model.restore_cached(f,200,200));
	Buf<256> rollback; model.status_line(rollback,100);
	CHECK(find(rollback.view(),"ahead of clock")!=sv::npos);
	CHECK(model.age(9223372036854775807LL)>0 && model.stale(9223372036854775807LL));
}
