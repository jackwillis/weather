// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
static void test_conditional_requests()
{
    constexpr const char *modified="Tue, 27 Oct 2026 12:00:00 GMT";
    HttpResponse h=http_parse("HTTP/1.0 200 OK\r\nLast-Modified: Tue, 27 Oct 2026 12:00:00 GMT\r\n\r\n{}");
    CHECK(h.st.ok() && h.last_modified==sv(modified));
    CHECK(!http_parse("HTTP/1.0 200 OK\r\nLast-Modified: a\r\nLast-Modified: b\r\n\r\n").st.ok());
    CHECK(!http_parse("HTTP/1.0 200 OK\r\nLast-Modified: bad\x01\r\n\r\n").st.ok());
    h=http_parse("HTTP/1.0 304 Not Modified\r\nContent-Length: 999\r\n\r\n");
    CHECK(h.st.ok() && h.code==304 && h.body.empty());
    CHECK(!http_parse("HTTP/1.0 304 Not Modified\r\n\r\nx").st.ok());
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

    // Open-Meteo never inherits MET validators, and still parses its normal 200 body.
    const Provider *om[]={&kOpenMeteo}; opts.conditional=&previous;
    snprintf(response,sizeof response,"HTTP/1.0 200 OK\r\nLast-Modified: %s\r\n\r\n%s",modified,fixture(0,"openmeteo-ok.json"));
    f.steps[0].response=response;
    CHECK(fetch_forecast(net,om,1,q,opts,buf,sizeof buf,&reused).ok());
    CHECK(!reused.last_modified[0] && strstr(f.request,"If-Modified-Since:")==nullptr);
}
