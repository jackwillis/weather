// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Seed-free native cache acceptance, confined to a new disposable directory.
#include "platform/os9/cache_os9.hpp"
#include <Events.h>
#include <cstdio>
#include <cstring>

namespace {
enum class Fault { None, Read, Write, Close, PreFlush, Exchange, PostFlush, Remove, FinalFlush, Rename };
Fault fault=Fault::None;
int failures=0,closes=0,flushes=0,removes=0;
FILE *logfile;
void check(bool ok,const char *label) {
	std::fprintf(logfile,"%s %s\n",ok?"ok":"FAIL",label);
	if(!ok) ++failures;
	std::fflush(logfile);
}
OSErr spec(const char *path,FSSpec &out) {
	unsigned char p[256]; size_t n=std::strlen(path); if(n>255) return paramErr;
	p[0]=static_cast<unsigned char>(n); std::memcpy(p+1,path,n); return FSMakeFSSpec(0,0,p,&out);
}
OSErr read(short fd,long *n,void *p) { if(fault==Fault::Read) return ioErr; return wx::os9_cache_file_ops().read(fd,n,p); }
OSErr write(short fd,long *n,const void *p) { if(fault==Fault::Write && *n) --*n; return wx::os9_cache_file_ops().write(fd,n,p); }
OSErr close(short fd) { OSErr e=wx::os9_cache_file_ops().close(fd); return fault==Fault::Close && ++closes==1?static_cast<OSErr>(ioErr):e; }
OSErr flush(short volume) {
	++flushes;
	if((fault==Fault::PreFlush && flushes==1) || (fault==Fault::PostFlush && flushes==2) || (fault==Fault::FinalFlush && flushes==3)) return ioErr;
	return wx::os9_cache_file_ops().flush(volume);
}
OSErr exchange(FSSpec *a,FSSpec *b) { return fault==Fault::Exchange?static_cast<OSErr>(ioErr):wx::os9_cache_file_ops().exchange(a,b); }
OSErr rename(FSSpec *a,const unsigned char *name) { return fault==Fault::Rename?static_cast<OSErr>(ioErr):wx::os9_cache_file_ops().rename(a,name); }
OSErr remove(FSSpec *a) { return fault==Fault::Remove && ++removes==1?static_cast<OSErr>(ioErr):wx::os9_cache_file_ops().remove(a); }
}
int main() {
	logfile=std::fopen("MacOS9Lives:weather-dev:cache-probe.log","w"); if(!logfile) return 2;
	char folder[180],path[220];
	std::snprintf(folder,sizeof folder,"MacOS9Lives:weather-dev:cache-check-%lu",static_cast<unsigned long>(TickCount()));
	FSSpec dir; long id=0;
	bool owned=spec(folder,dir)==fnfErr && FSpDirCreate(&dir,0,&id)==noErr;
	check(owned,"create private diagnostic directory");
	if(!owned) { std::fclose(logfile); return 1; }
	std::snprintf(path,sizeof path,"%s:forecast",folder);
	wx::CacheKey key{wx::Query{},"open-meteo"};
	wx::Forecast forecast; forecast.source="open-meteo"; forecast.temp_t10=650; forecast.code=0;
	constexpr long long at=1792950000LL;
	wx::Os9Cache store;
	check(wx::os9_cache_init(&store,path).ok(),"initialize diagnostic cache only");
	wx::CachedForecast cached; bool found=true;
	check(wx::read_cached_forecast(store.base,key,at,&cached,&found).ok() && !found,"missing cache is harmless");
	wx::Os9CacheFileOps ops{read,write,close,flush,exchange,rename,remove};
	fault=Fault::Rename;
	check(wx::os9_cache_init(&store,path,&ops).ok(),"initialize rename fault");
	check(!wx::write_cached_forecast(store.base,key,forecast,at).ok(),"rename failure is reported");
	fault=Fault::None;
	check(wx::os9_cache_init(&store,path).ok(),"initialize normal cache");
	check(wx::write_cached_forecast(store.base,key,forecast,at).ok(),"initial installation");
	forecast.temp_t10=600;
	check(wx::write_cached_forecast(store.base,key,forecast,at).ok(),"replace through same adapter instance");
	check(wx::read_cached_forecast(store.base,key,at,&cached,&found).ok() && found && cached.forecast.temp_t10==600,"same adapter reads replacement");
	Fault faults[]={Fault::Write,Fault::Close,Fault::PreFlush,Fault::Exchange,Fault::PostFlush,Fault::Remove,Fault::FinalFlush};
	for(Fault f:faults) {
		std::fprintf(logfile,"fault=%d\n",static_cast<int>(f));
		fault=Fault::None;
		check(wx::os9_cache_init(&store,path).ok(),"open baseline writer"); forecast.temp_t10=650;
		check(wx::write_cached_forecast(store.base,key,forecast,at).ok(),"write baseline");
		fault=f; closes=flushes=removes=0;
		check(wx::os9_cache_init(&store,path,&ops).ok(),"open faulted writer"); forecast.temp_t10=600;
		check(!wx::write_cached_forecast(store.base,key,forecast,at).ok(),"injected failure is reported");
		fault=Fault::None;
		check(wx::os9_cache_init(&store,path).ok(),"reopen after failure");
		check(wx::read_cached_forecast(store.base,key,at,&cached,&found).ok() && found,"valid primary survives");
		check(cached.forecast.temp_t10==(f==Fault::PostFlush || f==Fault::Remove || f==Fault::FinalFlush?600:650),"old or complete new record as expected");
	}
	for(Fault f:{Fault::Read,Fault::Close}) {
		fault=f; closes=0; check(wx::os9_cache_init(&store,path,&ops).ok(),"open faulted reader");
		check(!wx::read_cached_forecast(store.base,key,at,&cached,&found).ok() && !found,"read/close failure is not a cache hit");
	}
	fault=Fault::None;
	FSSpec file; check(spec(path,file)==noErr && FSpDelete(&file)==noErr,"remove diagnostic primary");
	check(FSpDelete(&dir)==noErr,"remove empty diagnostic directory");
	std::fprintf(logfile,"finished: %d failures\n",failures); std::fclose(logfile);
	return failures?1:0;
}
