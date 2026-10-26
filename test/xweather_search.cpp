// Explicitly linked fixture lookup; this file is absent from production Weather.
#include "ui/xweather/run.hpp"
#include <cstdio>
using namespace wx;
static Status fixture_search(Net &net,sv query,const FetchOpts &,char *,size_t,PlaceResults *out)
{
    puts("X11 fixture place lookup"); fflush(stdout);
    if(query=="Slow") {
        for(int i=0;i<300;i++) {
            if((net.yield && !net.yield(net)) || !net.sleep_ms(net,50)) { puts("X11 fixture search cancelled"); fflush(stdout); return make(Err::Cancelled,"fixture search cancelled"); }
        }
    }
    if(query=="Fail") return make(Err::HttpStatus,"Fixture search unavailable",503);
    if(query=="Empty") return geocoding_parse("{}",out);
    if(query=="Broken") return geocoding_parse("{",out);
    return geocoding_parse(R"({"results":[{"id":1,"name":"Madison","latitude":43.0748,"longitude":-89.3841,"admin1":"Wisconsin","country_code":"US"},{"id":2,"name":"Montr\u00e9al","latitude":45.5,"longitude":-73.5,"country_code":"CA"}]})",out);
}
int main(int argc,char **argv) { return run_xweather(argc,argv,fixture_search); }
