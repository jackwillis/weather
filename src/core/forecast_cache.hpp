// A bounded, canonical cache record. No native structs, pointers or host byte
// order are persisted. CRC detects corruption; this is not an authenticated file.
#pragma once
#include "provider.hpp"
#include <cstdint>

namespace wx {
constexpr size_t kForecastCacheBytes = 160;
constexpr long long kForecastCacheAge = 86400;
struct CacheKey { Query query; sv providers; };
struct CachedForecast { Forecast forecast; long long fetched_at = 0; };

namespace cache_detail {
inline uint32_t crc(const unsigned char *p, size_t n)
{
	uint32_t value=0xffffffffU;
	for(size_t i=0;i<n;i++) {
		value^=p[i];
		for(int bit=0;bit<8;bit++) value=(value>>1)^((value&1)?0xedb88320U:0);
	}
	return value^0xffffffffU;
}
inline void put32(unsigned char *p,uint32_t value)
{
	for(int i=3;i>=0;i--) { p[i]=static_cast<unsigned char>(value); value>>=8; }
}
inline uint32_t get32(const unsigned char *p)
{
	return (static_cast<uint32_t>(p[0])<<24)|(static_cast<uint32_t>(p[1])<<16)|(static_cast<uint32_t>(p[2])<<8)|p[3];
}
inline long signed32(const unsigned char *p)
{
	uint32_t n=get32(p);
	return static_cast<long>(n<=0x7fffffffU?static_cast<long long>(n):static_cast<long long>(n)-4294967296LL);
}
inline bool selected(sv list,sv provider)
{
	size_t pos=0;
	while(pos<list.size()) {
		size_t comma=find(list,",",pos);
		sv part=comma==sv::npos?list.substr(pos):list.substr(pos,comma-pos);
		if(trim(part)==provider) return true;
		if(comma==sv::npos) break;
		pos=comma+1;
	}
	return false;
}
inline bool bounded_text(const char *p,size_t cap,size_t *n)
{
	*n=0;
	while(*n<cap && p[*n]) {
		unsigned char c=static_cast<unsigned char>(p[*n]);
		if(c<32 || c>126) return false;
		++*n;
	}
	return *n<cap;
}
inline bool valid(const CacheKey &key,const Forecast &f)
{
	if(key.query.lat_e4 < -900000 || key.query.lat_e4>900000 || key.query.lon_e4 < -1800000 || key.query.lon_e4>1800000) return false;
	if(f.metric!=key.query.metric || key.providers.empty() || key.providers.size()>=64) return false;
	for(char c:key.providers) if(c<32 || c>126) return false;
	if(!f.source || (sv(f.source)!="open-meteo" && sv(f.source)!="met-no" && sv(f.source)!="nws")) return false;
	if(!selected(key.providers,f.source)) return false;
	if(!plausible_temp_t10(f.temp_t10,f.metric)) return false;
	bool known=sv(f.source)=="nws" && f.code==-1; for(const WmoName &name:kWmo) if(name.code==f.code) known=true;
	if(!known) return false;
	if(f.humidity!=kUnset && (f.humidity<0 || f.humidity>100)) return false;
	if(f.wind_t10!=kUnset && (f.wind_t10<0 || f.wind_t10>5000)) return false;
	if(f.wind_dir!=kUnset && (f.wind_dir<0 || f.wind_dir>360)) return false;
	if(f.is_day!=kUnset && f.is_day!=0 && f.is_day!=1) return false;
	size_t n=0; return bounded_text(f.time,sizeof f.time,&n);
}
}

// Layout: magic[8], lat/lon[8], metric/source[2], reserved[6], fetched[8],
// six signed fields[24], observation[24], provider selection[64], reserved[12], CRC[4].
inline bool cache_encode(const CacheKey &key,const Forecast &f,long long at,unsigned char *out)
{
	if(at<0 || !cache_detail::valid(key,f)) return false;
	for(size_t i=0;i<kForecastCacheBytes;i++) out[i]=0;
	const char magic[]="WXFCACH1";
	for(size_t i=0;i<8;i++) out[i]=magic[i];
	cache_detail::put32(out+8,static_cast<uint32_t>(key.query.lat_e4));
	cache_detail::put32(out+12,static_cast<uint32_t>(key.query.lon_e4));
	out[16]=f.metric?1:0; out[17]=sv(f.source)=="open-meteo"?1:sv(f.source)=="met-no"?2:3;
	uint64_t stamp=static_cast<uint64_t>(at);
	for(int i=31;i>=24;i--) { out[i]=static_cast<unsigned char>(stamp); stamp>>=8; }
	long fields[]={f.temp_t10,f.humidity,f.wind_t10,f.wind_dir,f.code,f.is_day};
	for(size_t i=0;i<6;i++) cache_detail::put32(out+32+4*i,static_cast<uint32_t>(fields[i]));
	for(size_t i=0;i<sizeof f.time && f.time[i];i++) out[56+i]=static_cast<unsigned char>(f.time[i]);
	for(size_t i=0;i<key.providers.size();i++) out[80+i]=static_cast<unsigned char>(key.providers[i]);
	cache_detail::put32(out+156,cache_detail::crc(out,156));
	return true;
}

inline bool cache_decode(const unsigned char *p,size_t n,const CacheKey &key,long long now,CachedForecast *out)
{
	if(n!=kForecastCacheBytes || now<0 || key.providers.empty() || key.providers.size()>=64) return false;
	const char magic[]="WXFCACH1";
	for(size_t i=0;i<8;i++) if(p[i]!=static_cast<unsigned char>(magic[i])) return false;
	if(cache_detail::crc(p,156)!=cache_detail::get32(p+156)) return false;
	for(size_t i=18;i<24;i++) if(p[i]) return false;
	for(size_t i=144;i<156;i++) if(p[i]) return false;
	if(p[16]>1 || p[17]<1 || p[17]>3) return false;
	if(cache_detail::signed32(p+8)!=key.query.lat_e4 || cache_detail::signed32(p+12)!=key.query.lon_e4 || (p[16]!=0)!=key.query.metric) return false;
	for(size_t i=0;i<64;i++) {
		unsigned char expected=i<key.providers.size()?static_cast<unsigned char>(key.providers[i]):0;
		if(p[80+i]!=expected) return false;
	}
	uint64_t stamp=0;
	for(size_t i=24;i<32;i++) stamp=(stamp<<8)|p[i];
	if(stamp>9223372036854775807ULL) return false;
	long long at=static_cast<long long>(stamp);
	if(at>now || now-at>kForecastCacheAge) return false;
	CachedForecast decoded; decoded.fetched_at=at;
	Forecast &f=decoded.forecast;
	f.metric=p[16]!=0; f.source=p[17]==1?"open-meteo":p[17]==2?"met-no":"nws";
	f.temp_t10=cache_detail::signed32(p+32); f.humidity=cache_detail::signed32(p+36);
	f.wind_t10=cache_detail::signed32(p+40); f.wind_dir=cache_detail::signed32(p+44);
	f.code=cache_detail::signed32(p+48); f.is_day=cache_detail::signed32(p+52);
	bool ended=false;
	for(size_t i=0;i<sizeof f.time;i++) {
		if(!p[56+i]) ended=true;
		else if(ended) return false;
		f.time[i]=static_cast<char>(p[56+i]);
	}
	if(!cache_detail::valid(key,f)) return false;
	*out=decoded;
	return true;
}
} // namespace wx
