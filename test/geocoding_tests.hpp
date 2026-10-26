#include "core/geocoding.hpp"
#include "app/location_search.hpp"
static Status place_tls(Net &,Io &raw,const char *host,Io *out) { CHECK(!strcmp(host,"geocoding-api.open-meteo.com")); *out=raw; return okay(); }
static void place_tls_close(Net &,Io &) {}
static bool place_cancel(Net &) { return false; }
static void test_geocoding()
{
    char path[256];
    CHECK(geocoding_path(" Madison, WI ",path,sizeof path).ok());
    CHECK(!strcmp(path,"/v1/search?name=Madison%2C%20WI&count=5&language=en&format=json"));
    CHECK(!geocoding_path("x",path,sizeof path).ok());
    CHECK(!geocoding_path("A\nB",path,sizeof path).ok());
    CHECK(!geocoding_path("Paris",path,8).ok());
    CHECK(geocoding_path("A&B?",path,sizeof path).ok());
    CHECK(strstr(path,"A%26B%3F")!=nullptr);
    const char *good=R"({"results":[{"id":1,"name":"Madison","latitude":43.07476,"longitude":-89.38414,"admin1":"Wisconsin","country_code":"US"},{"id":2,"name":"Paris","latitude":48.85,"longitude":2.35,"country_code":"FR"}]})";
    PlaceResults results;
    CHECK(geocoding_parse(good,&results).ok());
    CHECK(results.count==2 && results.items[0].lat_e4==430748 && results.items[0].lon_e4==-893841);
    CHECK(!strcmp(results.items[0].name,"Madison") && !strcmp(results.items[0].region,"Wisconsin"));
    Buf<2048> listing; format_places(listing,results);
    CHECK(find(listing.view(),"Open-Meteo / GeoNames")!=sv::npos);
    CHECK(find(listing.view(),"1\tMadison\tWisconsin US\t43.0748,-89.3841")!=sv::npos);
    CHECK(find(listing.view(),"2\tParis\tFR\t48.8500,2.3500")!=sv::npos && !listing.truncated);
    CHECK(geocoding_parse("{}",&results).ok() && results.count==0);
    CHECK(geocoding_parse("{\"results\":[]}",&results).ok() && results.count==0);
    const char *bad[]={"{", "{\"results\":null}", "{\"error\":true}",
        R"({"results":[{"id":1,"name":"Bad","latitude":91,"longitude":0}]})",
        R"({"results":[{"id":1,"name":"Bad","latitude":0}]})",
        R"({"results":[{"id":0,"name":"Bad","latitude":0,"longitude":0}]})",
        R"({"results":[{"id":1,"name":"\u0000","latitude":0,"longitude":0}]})",
        R"({"results":[{"id":1,"name":"\ud800","latitude":0,"longitude":0}]})",
        R"({"ignored":garbage})",R"({"ignored":01})",R"({"results":[],"ignored":"\q"})"};
    for(const char *text:bad) {
        results.count=99;
        CHECK(!geocoding_parse(text,&results).ok());
        CHECK(results.count==99);
    }
    CHECK(geocoding_parse(R"({"results":[{"id":1,"name":"Montr\u00e9al","latitude":45.5,"longitude":-73.5}]})",&results).ok());
    CHECK(!strcmp(results.items[0].name,"Montr?al") && results.items[0].simplified);
    char excessive[kGeocodingBodyLimit+1]; memset(excessive,' ',sizeof excessive);
    results.count=99;
    CHECK(!geocoding_parse(sv(excessive,sizeof excessive),&results).ok() && results.count==99);
    char long_query[65]; memset(long_query,'x',64); long_query[64]=0;
    CHECK(!geocoding_path(long_query,path,sizeof path).ok());
    const char *item=R"({"id":1,"name":"Town","latitude":0,"longitude":0})";
    Buf<1024> many; many.add("{\"results\":[");
    for(int i=0;i<6;i++) { if(i) many.add(','); many.add(item); } many.add("]}");
    CHECK(!geocoding_parse(many.view(),&results).ok() && results.count==99);
    CHECK(!geocoding_parse(R"({"results":[{"id":1.2,"name":"Town","latitude":0,"longitude":0}]})",&results).ok());
    CHECK(!geocoding_parse(R"({"results":[{"id":1,"name":"A name that is deliberately longer than forty seven characters","latitude":0,"longitude":0}]})",&results).ok());
    CHECK(!geocoding_parse("{\"results\":[{\"id\":1,\"name\":\"\xc0\xaf\",\"latitude\":0,\"longitude\":0}]}",&results).ok());
    LocationSearch search; Config cfg; LocationForm form; form.load(cfg);
    CHECK(!search.choose(0,form).ok());
    CHECK(search.accept(good).ok() && search.results.count==2);
    CHECK(!search.choose(2,form).ok());
    CHECK(search.choose(1,form).ok());
    CHECK(form.lat.view()=="48.8500" && form.lon.view()=="2.3500" && form.name.view()=="Paris");
    CHECK(form.metric==cfg.metric && cfg.lat_e4==Config{}.lat_e4);
    PrefMemory prefs, legacy; Model model; Forecast old; old.temp_t10=123;
    CHECK(model.tick(0)); model.finish(1,okay(),&old);
    Config selected=cfg; CHECK(form.read(&selected).ok());
    prefs.fail_save=true;
    CHECK(!commit_preferences(prefs.base,legacy.base,selected,cfg,model,2).ok());
    CHECK(cfg.lat_e4==Config{}.lat_e4 && model.has_forecast());
    prefs.fail_save=false;
    CHECK(commit_preferences(prefs.base,legacy.base,selected,cfg,model,3).ok());
    CHECK(cfg.lat_e4==488500 && !model.has_forecast());
    Config restored; CHECK(load_preferences(prefs.base,legacy.base,restored).ok());
    CHECK(restored.lat_e4==488500 && restored.lon_e4==23500);
    PlacePicker picker; LocationForm picked; picked.load(cfg);
    CHECK(!picker.choose(picked).ok());
    CHECK(!picker.begin("x").ok() && !picker.busy);
    CHECK(picker.begin("Paris").ok() && picker.busy);
    CHECK(!picker.begin("Madison").ok());
    CHECK(!picker.select(0).ok() && !picker.choose(picked).ok());
    picker.finish(okay(),search.results);
    CHECK(!picker.busy && picker.selected==-1 && !picker.choose(picked).ok());
    CHECK(!picker.select(2).ok() && picker.select(1).ok());
    CHECK(picker.choose(picked).ok() && picked.name.view()=="Paris");
    picker.edited(); CHECK(picker.selected==-1 && picker.search.results.count==0 && !picker.choose(picked).ok());
    CHECK(picker.begin("Paris").ok()); picker.cancel(); picker.finish(okay(),search.results);
    CHECK(picker.search.error.err==Err::Cancelled && !picker.choose(picked).ok() && picker.search.results.count==0);
    CHECK(picker.begin("Paris").ok()); picker.finish(make(Err::HttpStatus,"fixture",503),search.results);
    CHECK(picker.search.results.count==0 && picker.search.error.err==Err::HttpStatus);
    CHECK(picker.begin("Paris").ok()); PlaceResults oversized; oversized.count=kPlaceLimit+1;
    picker.finish(okay(),oversized); CHECK(picker.search.error.err==Err::BadData && picker.search.results.count==0);
    char response[4096], buffer[8192]; snprintf(response,sizeof response,"HTTP/1.0 200 OK\r\n\r\n%s",good);
    Fake fake; fake.steps[0]={okay(),response,Err::Ok}; fake.nsteps=1;
    Net net=make_net(&fake); net.tls_open=place_tls; net.tls_close=place_tls_close;
    FetchOpts options; options.attempts=2;
    CHECK(search_places(net,"Madison",options,buffer,sizeof buffer,&results).ok() && results.count==2);
    CHECK(strstr(fake.request,"Host: geocoding-api.open-meteo.com")!=nullptr);
    net.yield=place_cancel; int before=fake.connects;
    CHECK(search_places(net,"Madison",options,buffer,sizeof buffer,&results).err==Err::Cancelled && fake.connects==before);
    net.yield=nullptr;
    fake.steps[0]={okay(),"HTTP/1.0 429 Limited\r\n\r\n",Err::Ok};
    results.count=99; before=fake.connects;
    CHECK(search_places(net,"Madison",options,buffer,sizeof buffer,&results).err==Err::HttpStatus);
    CHECK(fake.connects==before+2 && fake.nsleeps==1 && results.count==99);
    net.tls_open=nullptr;
    CHECK(search_places(net,"Madison",options,buffer,sizeof buffer,&results).err==Err::Tls);
    search.clear(); CHECK(!search.choose(0,form).ok());
    CHECK(search.accept(good).ok());
    CHECK(!search.accept("bad").ok() && search.results.count==0);
}
