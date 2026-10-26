// A result is copied into editable fields only after an explicit choice.
#pragma once
#include "locform.hpp"
#include "core/geocoding.hpp"
namespace wx {
struct LocationSearch {
    PlaceResults results;
    Status error;
    void clear() { results=PlaceResults{}; error=okay(); }
    Status accept(sv body) { clear(); error=geocoding_parse(body,&results); return error; }
    Status choose(size_t index,LocationForm &form) const {
        if(index>=results.count) return make(Err::Usage,"Choose a place result first");
        const Place &p=results.items[index]; Buf<16> lat,lon;
        lat.add_fixed(p.lat_e4,4); lon.add_fixed(p.lon_e4,4);
        form.lat.set(lat.view()); form.lon.set(lon.view()); form.name.set(p.name);
        form.error=""; return okay(); // Existing Apply/save is still required.
    }
};
}
