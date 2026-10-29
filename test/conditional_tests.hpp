// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
static void test_conditional_requests()
{
    Config defaults;
    CHECK(sv(defaults.providers)=="open-meteo");   // MET fallback is opt-in for v0.1 (#152 moved to v0.2)
    constexpr const char *modified="Tue, 27 Oct 2026 12:00:00 GMT";
    long long expiry=0;
    CHECK(http_date("Tue, 27 Oct 2026 12:00:00 GMT",&expiry) && expiry==1793102400LL);
    CHECK(http_date("Thu, 01 Jan 1970 00:00:00 GMT",&expiry) && expiry==0);
    CHECK(!http_date("Tue, 29 Feb 2026 12:00:00 GMT",&expiry));
    CHECK(!http_date("Tue, 27 Oct 2026 24:00:00 GMT",&expiry));
    CHECK(!http_date("Tue, 27 Oct 2026 12:00:00 GMT trailing",&expiry));
    CHECK(!http_parse("HTTP/1.0 200 OK\r\nExpires: bad\r\n\r\n").st.ok());
    CHECK(!http_parse("HTTP/1.0 200 OK\r\nExpires: Tue, 27 Oct 2026 12:00:00 GMT\r\nExpires: Tue, 27 Oct 2026 12:00:00 GMT\r\n\r\n").st.ok());
    HttpResponse h=http_parse("HTTP/1.0 200 OK\r\nLast-Modified: Tue, 27 Oct 2026 12:00:00 GMT\r\n\r\n{}");
    CHECK(h.st.ok() && h.last_modified==sv(modified));
    CHECK(!http_parse("HTTP/1.0 200 OK\r\nLast-Modified: a\r\nLast-Modified: b\r\n\r\n").st.ok());
    CHECK(!http_parse("HTTP/1.0 200 OK\r\nLast-Modified: bad\x01\r\n\r\n").st.ok());
    h=http_parse("HTTP/1.0 304 Not Modified\r\nContent-Length: 999\r\n\r\n");
    CHECK(h.st.ok() && h.code==304 && h.body.empty());
    CHECK(!http_parse("HTTP/1.0 304 Not Modified\r\n\r\nx").st.ok());
    CHECK(!http_parse("HTTP/1.0 304 Not Modified\r\nContent-Length: -1\r\n\r\n").st.ok());
    CHECK(!http_parse("HTTP/1.0 304 Not Modified\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\n").st.ok());
    CHECK(!http_parse("HTTP/1.0 304 Not Modified\r\nTransfer-Encoding: identity\r\n\r\n").st.ok());
    Buf<512> request;
    CHECK(http_request(request,"api.met.no","/","fixture",modified));
    CHECK(find(request.view(),"If-Modified-Since: Tue, 27 Oct 2026 12:00:00 GMT\r\n")!=sv::npos);
    request=Buf<512>{}; CHECK(!http_request(request,"h","/","fixture","date\r\nInjected: yes"));

    Query q; Config cfg; strcpy(cfg.providers,"met-no");
    q.lat_e4=cfg.lat_e4; q.lon_e4=cfg.lon_e4;
    const Provider *only[]={&kMetNo};
    FetchOpts opts; opts.attempts=1; opts.force_host="fake"; opts.contact="fixture.invalid";
    static char response[8192];
    snprintf(response,sizeof response,"HTTP/1.0 200 OK\r\nLast-Modified: %s\r\n\r\n%s",modified,fixture(0,"metno-ok.json"));
    Fake f; f.steps[0]={okay(),response,Err::Ok}; f.nsteps=1;
    Net net=make_net(&f); char buf[8192]; Forecast fresh;
    CHECK(fetch_forecast(net,only,1,q,opts,buf,sizeof buf,&fresh).ok());
    CHECK(!strcmp(fresh.last_modified,modified));
    opts.contact=nullptr;
    CHECK(fetch_forecast(net,only,1,q,opts,buf,sizeof buf,&fresh).ok());
    CHECK(strstr(f.request,"https://github.com/jackwillis/weather")!=nullptr);
    opts.contact="fixture.invalid";
    CHECK(fetch_forecast(net,only,1,q,opts,buf,sizeof buf,&fresh).ok());
    CHECK(strstr(f.request,"contact: fixture.invalid")!=nullptr);
    CHECK(strstr(f.request,"github.com")==nullptr);
    ConditionalForecast previous{q,fresh}; opts.conditional=&previous;
    f.steps[0].response="HTTP/1.0 304 Not Modified\r\nContent-Length: 999\r\n\r\n";
    Forecast reused;
    CHECK(fetch_forecast(net,only,1,q,opts,buf,sizeof buf,&reused).ok());
    CHECK(reused.temp_t10==fresh.temp_t10 && !strcmp(reused.last_modified,modified));
    CHECK(strstr(f.request,"If-Modified-Since: Tue, 27 Oct 2026 12:00:00 GMT\r\n")!=nullptr);
    previous.query.lat_e4++;
    CHECK(fetch_forecast(net,only,1,q,opts,buf,sizeof buf,&reused).err==Err::HttpStatus);
    CHECK(strstr(f.request,"If-Modified-Since")==nullptr);
    previous.query=q; previous.forecast.metric=true;
    CHECK(fetch_forecast(net,only,1,q,opts,buf,sizeof buf,&reused).err==Err::HttpStatus);
    previous.forecast=fresh; strcpy(previous.forecast.last_modified,"bad\r\nInjected: yes");
    CHECK(fetch_forecast(net,only,1,q,opts,buf,sizeof buf,&reused).err==Err::HttpStatus);
    opts.conditional=nullptr;
    CHECK(fetch_forecast(net,only,1,q,opts,buf,sizeof buf,&reused).err==Err::HttpStatus);

    // A serialized cache read, not an in-memory Model pointer, supplies the next validator.
    unsigned char record[kForecastCacheBytes];
    CHECK(cache_encode(cache_key(cfg),fresh,f.clock,record));
    CachedForecast decoded;
    CHECK(cache_decode(record,sizeof record,cache_key(cfg),f.clock,&decoded));
    CHECK(!strcmp(decoded.forecast.last_modified,modified));
    CacheStore store; store.ctx=record;
    store.load=[](CacheStore &s,unsigned char *out,size_t cap,size_t *size) {
        *size=kForecastCacheBytes;
        if(cap<*size) return make(Err::Io,"small fixture buffer");
        memcpy(out,s.ctx,*size); return okay();
    };
    opts.force_host=nullptr;
    net.tls_open=[](Net &,Io &raw,const char *,Io *out) { *out=raw; return okay(); };
    net.tls_close=[](Net &,Io &) {};
    CHECK(fetch_forecast_cached(net,only,1,q,opts,buf,sizeof buf,&reused,&store,cfg.providers).ok());
    CHECK(strstr(f.request,"If-Modified-Since:")!=nullptr);
    // Plain HTTP fixtures never load a persistent validator.
    opts.force_host="fake";
    CHECK(fetch_forecast_cached(net,only,1,q,opts,buf,sizeof buf,&reused,&store,cfg.providers).err==Err::HttpStatus);
    CHECK(strstr(f.request,"If-Modified-Since:")==nullptr);

    // An unexpired MET response is reused without opening a socket; at expiry
    // it revalidates. A 304 replaces expiry instead of extending it locally.
    opts.force_host=nullptr; opts.conditional=&previous;
    previous.forecast=fresh; previous.forecast.expires=f.clock+900;
    int before=f.connects;
    CHECK(fetch_forecast(net,only,1,q,opts,buf,sizeof buf,&reused).ok());
    CHECK(f.connects==before && reused.expires==f.clock+900);
    f.clock+=900;
    f.steps[0].response="HTTP/1.0 304 Not Modified\r\nExpires: Tue, 27 Oct 2026 12:00:00 GMT\r\n\r\n";
    CHECK(fetch_forecast(net,only,1,q,opts,buf,sizeof buf,&reused).ok());
    CHECK(f.connects==before+1 && reused.expires==1793102400LL);
    previous.query.lat_e4++;
    CHECK(fetch_forecast(net,only,1,q,opts,buf,sizeof buf,&reused).err==Err::HttpStatus);
    previous.query=q;
    previous.forecast.expires=f.clock+86401; previous.fetched_at=f.clock-86401;
    before=f.connects;
    CHECK(fetch_forecast(net,only,1,q,opts,buf,sizeof buf,&reused).err==Err::NoSource);
    CHECK(f.connects==before);
    previous.fetched_at=-1; opts.force_host="fake";

    // Open-Meteo never inherits MET validators, and still parses its normal 200 body.
    const Provider *om[]={&kOpenMeteo}; opts.conditional=&previous;
    snprintf(response,sizeof response,"HTTP/1.0 200 OK\r\nLast-Modified: %s\r\n\r\n%s",modified,fixture(0,"openmeteo-ok.json"));
    f.steps[0].response=response;
    CHECK(fetch_forecast(net,om,1,q,opts,buf,sizeof buf,&reused).ok());
    CHECK(!reused.last_modified[0] && strstr(f.request,"If-Modified-Since:")==nullptr);
    // Saving defaults on; off forbids persistence for every provider.
    strcpy(cfg.providers,"open-meteo,met-no"); strcpy(cfg.contact,"fixture.invalid");
    CHECK(cfg.cache && cache_storage_needed(cfg));
    struct Writes { unsigned count=0; size_t size=0; unsigned char record[2*kForecastCacheBytes]={}; } writes;
    CacheStore tracked; tracked.ctx=&writes;
    tracked.save=[](CacheStore &s,const unsigned char *bytes,size_t size) {
        auto &state=*static_cast<Writes *>(s.ctx);
        if(size>sizeof state.record) return make(Err::Io,"wrong fixture length");
        memcpy(state.record,bytes,size); state.size=size; ++state.count; return okay();
    };
    tracked.load=[](CacheStore &s,unsigned char *out,size_t cap,size_t *size) {
        auto &state=*static_cast<Writes *>(s.ctx); *size=state.size;
        if(cap<*size) return make(Err::Io,"small fixture buffer");
        memcpy(out,state.record,*size); return okay();
    };
    CHECK(update_forecast_cache(tracked,cache_key(cfg),fresh,f.clock,cfg.cache).ok());
    CHECK(writes.count==1 && cache_decode(writes.record,kForecastCacheBytes,cache_key(cfg),f.clock,&decoded));
    CHECK(!strcmp(decoded.forecast.last_modified,modified));
    fresh.expires=f.clock+3600;
    CHECK(update_forecast_cache(tracked,cache_key(cfg),fresh,f.clock,cfg.cache).ok());
    Forecast open=fresh; open.expires=0; open.source="open-meteo"; open.last_modified[0]=0;
    CHECK(update_forecast_cache(tracked,cache_key(cfg),open,f.clock,cfg.cache).ok());
    CHECK(writes.count==3);
    // The primary remains the latest OM forecast, with MET retained separately.
    CHECK(read_cached_met(tracked,cache_key(cfg),f.clock,&decoded).ok());
    CHECK(decoded.forecast.expires==fresh.expires && writes.size==2*kForecastCacheBytes);
    bool found=false;
    CHECK(read_cached_forecast(tracked,cache_key(cfg),f.clock,&decoded,&found).ok() && found);
    CHECK(sv(decoded.forecast.source)=="open-meteo");
    opts.save_forecasts=true; opts.force_host=nullptr; opts.conditional=nullptr;
    before=f.connects;
    CHECK(fetch_forecast_cached(net,only,1,q,opts,buf,sizeof buf,&reused,&tracked,cfg.providers).ok());
    CHECK(f.connects==before && reused.saved_valid && reused.expires==fresh.expires);
    unsigned saved_count=writes.count;
    CHECK(update_forecast_cache(tracked,cache_key(cfg),reused,f.clock+1,true).ok() && writes.count==saved_count);
    Model saved_model; CHECK(saved_model.tick(f.clock)); saved_model.finish(f.clock,okay(),&reused);
    Buf<256> status; saved_model.status_line(status,f.clock);
    CHECK(find(status.view(),"Saved forecast, server-valid")!=sv::npos);
    // Saving off skips even an otherwise valid unexpired MET entry.
    opts.save_forecasts=false;
    CHECK(fetch_forecast_cached(net,only,1,q,opts,buf,sizeof buf,&reused,&tracked,cfg.providers).err==Err::Config);
    CHECK(f.connects==before);
    cfg.cache=false;
    CHECK(update_forecast_cache(tracked,cache_key(cfg),fresh,f.clock,cfg.cache).ok() && writes.count==3);
    cfg.cache=false; cfg.contact[0]=0;
    CHECK(!cache_storage_needed(cfg));
    strcpy(cfg.contact,"fixture.invalid"); strcpy(cfg.providers,"open-meteo");
    CHECK(!cache_storage_needed(cfg));

    opts.save_forecasts=false;
    strcpy(f.request,"untouched");
    Status disabled=fetch_forecast(net,only,1,q,opts,buf,sizeof buf,&reused);
    CHECK(disabled.err==Err::Config && strstr(disabled.msg,"forecast saving"));
    CHECK(!strcmp(f.request,"untouched"));
    CHECK(fetch_forecast(net,om,1,q,opts,buf,sizeof buf,&reused).ok());
    const Provider *fallback[]={&kMetNo,&kOpenMeteo};
    CHECK(fetch_forecast(net,fallback,2,q,opts,buf,sizeof buf,&reused).ok());
    CHECK(sv(reused.source)=="open-meteo");
    unsigned reads=0; store.ctx=&reads;
    store.load=[](CacheStore &s,unsigned char *,size_t,size_t *) {
        ++*static_cast<unsigned *>(s.ctx); return make(Err::Io,"unexpected read");
    };
    opts.force_host=nullptr;
    CHECK(fetch_forecast_cached(net,om,1,q,opts,buf,sizeof buf,&reused,&store,"open-meteo").ok());
    CHECK(reads==0);
    // A skipped MET choice must not hide the last attempted provider's failure.
    f.steps[0]={make(Err::Dns,"fixture DNS failure"),"",Err::Ok}; f.nsteps=1; f.next=0;
    opts.force_host="fake";
    const Provider *attempt_then_skip[]={&kOpenMeteo,&kMetNo};
    CHECK(fetch_forecast(net,attempt_then_skip,2,q,opts,buf,sizeof buf,&reused).err==Err::Dns);

}
