// NWS observations: point -> closest listed station -> latest observation.
// Three bounded responses share the caller's buffer; only the next path survives.
#pragma once
#include "json.hpp"
#include "provider.hpp"

namespace wx {

constexpr bool nws_copy(sv text, char *out, size_t cap)
{
    if (text.size() + 1 > cap) return false;
    for (size_t i = 0; i < text.size(); ++i) out[i] = text[i];
    out[text.size()] = 0;
    return true;
}

// Deliberately narrow subset: no credentials, escapes, query, fragment, dot
// segments, port changes or alternate authorities. Never use returned authority.
constexpr bool nws_url_path(sv url, sv *path)
{
    constexpr sv base = "https://api.weather.gov";
    if (url.substr(0, base.size()) != base) return false;
    url.remove_prefix(base.size());
    if (url.substr(0, 4) == sv(":443")) url.remove_prefix(4);
    if (url.empty() || url[0] != '/' || url.size() > 255) return false;
    for (size_t i = 0; i < url.size(); ++i) {
        char c = url[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '/' || c == ',' || c == '-')) return false;
        if (c == '/' && i && url[i - 1] == '/') return false;
    }
    *path = url;
    return true;
}

inline bool nws_path(const Query &q, char *out, size_t cap)
{
    if (q.lat_e4 < -900000 || q.lat_e4 > 900000 || q.lon_e4 < -1800000 || q.lon_e4 > 1800000) return false;
    Buf<80> b;
    b.add("/points/").add_fixed(q.lat_e4, 4).add(',').add_fixed(q.lon_e4, 4);
    return !b.truncated && nws_copy(b.view(), out, cap);
}

inline Status nws_next(sv body, unsigned step, char *out, size_t cap)
{
    JsonLookup value;
    if (step == 1) {
        JsonLookup props = json_get(body, "properties");
        if (props.r != JsonFind::Found) return make(Err::BadData, "NWS point has no properties");
        value = json_get(props.value, "observationStations");
    } else if (step == 2) {
        JsonLookup features = json_get(body, "features");
        if (features.r != JsonFind::Found) return make(Err::BadData, "NWS station list missing");
        JsonLookup first = json_first(features.value);
        if (first.r != JsonFind::Found) return make(Err::BadData, "NWS has no nearby station");
        value = json_get(first.value, "id");
    } else return make(Err::BadData, "invalid NWS request step");
    sv url, path;
    if (value.r != JsonFind::Found || !json_str(value.value, &url) || !nws_url_path(url, &path))
        return make(Err::BadData, "unsafe or missing NWS URL");
    Buf<320> next;
    if (step == 1) {
        constexpr sv prefix = "/gridpoints/", suffix = "/stations";
        if (path.substr(0, prefix.size()) != prefix || path.size() <= prefix.size() + suffix.size() ||
            path.substr(path.size() - suffix.size()) != suffix)
            return make(Err::BadData, "unexpected NWS station-list path");
        // One office and one grid coordinate pair, no arbitrary endpoint chain.
        sv grid = path.substr(prefix.size(), path.size() - prefix.size() - suffix.size());
        size_t slash = find(grid, "/");
        if (slash == sv::npos || slash == 0 || find(grid.substr(slash + 1), "/") != sv::npos ||
            find(grid.substr(slash + 1), ",") == sv::npos)
            return make(Err::BadData, "invalid NWS grid path");
        next.add(path).add("?limit=1");
    } else {
        constexpr sv prefix = "/stations/";
        if (path.substr(0, prefix.size()) != prefix) return make(Err::BadData, "unexpected NWS station path");
        sv id = path.substr(prefix.size());
        if (id.empty() || id.size() > 16) return make(Err::BadData, "invalid NWS station identifier");
        for (char c : id)
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
                return make(Err::BadData, "invalid NWS station identifier");
        next.add(path).add("/observations/latest");
    }
    if (next.truncated || !nws_copy(next.view(), out, cap)) return make(Err::BadData, "NWS request path too long");
    return okay();
}

// A null or absent optional measurement stays unset. Present measurements must
// have the expected unit and bounded value before any arithmetic or conversion.
constexpr Status nws_measure(sv props, sv key, sv unit, long low, long high, bool required, long *out)
{
    JsonLookup field = json_get(props, key);
    if (field.r == JsonFind::Absent && !required) { *out = kUnset; return okay(); }
    if (field.r != JsonFind::Found) return make(Err::BadData, "NWS measurement missing");
    JsonLookup value = json_get(field.value, "value");
    if (value.r == JsonFind::Found && value.value == sv("null") && !required) { *out = kUnset; return okay(); }
    JsonLookup units = json_get(field.value, "unitCode");
    sv u;
    if (value.r != JsonFind::Found || units.r != JsonFind::Found || !json_str(units.value, &u) || u != unit)
        return make(Err::BadData, "NWS measurement has unexpected units");
    Num n = json_num(value.value, 1);
    if (!n.ok || n.v < low || n.v > high) return make(Err::BadData, "NWS measurement out of range");
    *out = n.v;
    return okay();
}

// Bounded array iteration, with whole-array structural validation first.
// Parser callbacks get only one item's view; no allocation or retained pointers.
template <class F>
inline bool nws_each(sv array, F fn)
{
    size_t i = json_ws(array, 0), end = json_skip(array, i);
    if (i >= array.size() || array[i] != '[' || end == sv::npos || json_ws(array, end) != array.size()) return false;
    i = json_ws(array, i + 1);
    for (unsigned count = 0; i < array.size() && array[i] != ']'; ++count) {
        if (count == 16) return false;
        size_t e = json_skip(array, i);
        if (e == sv::npos || !fn(array.substr(i, e - i))) return false;
        i = json_ws(array, e);
        if (i < array.size() && array[i] == ',') i = json_ws(array, i + 1);
    }
    return true;
}

inline Status nws_parse(sv body, const Query &q, Forecast *out)
{
    JsonLookup props = json_get(body, "properties");
    if (props.r != JsonFind::Found) return make(Err::BadData, "NWS observation properties missing");
    Forecast f;
    long temp, wind, humidity, dir;
    Status s = nws_measure(props.value, "temperature", "wmoUnit:degC", -900, 600, true, &temp);
    if (!s.ok()) return s;
    s = nws_measure(props.value, "windSpeed", "wmoUnit:km_h-1", 0, 5000, false, &wind);
    if (!s.ok()) return s;
    s = nws_measure(props.value, "relativeHumidity", "wmoUnit:percent", 0, 1000, false, &humidity);
    if (!s.ok()) return s;
    s = nws_measure(props.value, "windDirection", "wmoUnit:degree_(angle)", 0, 3600, false, &dir);
    if (!s.ok()) return s;
    f.temp_t10 = q.metric ? temp : (temp * 9 + (temp >= 0 ? 2 : -2)) / 5 + 320;
    if (wind != kUnset) f.wind_t10 = q.metric ? wind : (wind * 62137 + 50000) / 100000;
    if (humidity != kUnset) f.humidity = (humidity + 5) / 10;
    if (dir != kUnset) f.wind_dir = (dir + 5) / 10 % 360;
    // Only UTC timestamps are accepted; formatting elsewhere labels them UTC.
    JsonLookup stamp = json_get(props.value, "timestamp");
    sv time;
    if (stamp.r != JsonFind::Found || !json_str(stamp.value, &time) ||
        (!(time.size() == 20 && time.substr(19) == sv("Z")) &&
         !(time.size() == 25 && time.substr(19) == sv("+00:00"))))
        return make(Err::BadData, "NWS timestamp must be UTC");
    for (size_t i = 0; i < 19; ++i) {
        char expected = i == 4 || i == 7 ? '-' : i == 10 ? 'T' : i == 13 || i == 16 ? ':' : 0;
        if (expected ? time[i] != expected : time[i] < '0' || time[i] > '9') return make(Err::BadData, "invalid NWS timestamp");
    }
    auto two = [&](size_t i) { return (time[i] - '0') * 10 + time[i + 1] - '0'; };
    int year = two(0) * 100 + two(2), month = two(5), day = two(8);
    constexpr int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    if (year < 2025 || month < 1 || month > 12 || day < 1 ||
        day > days[month - 1] + (month == 2 && leap ? 1 : 0) ||
        two(11) > 23 || two(14) > 59 || two(17) > 59)
        return make(Err::BadData, "invalid NWS calendar date");
    nws_copy(time.substr(0, 16), f.time, sizeof f.time);
    f.code = -1; // Missing/unrecognised conditions are explicitly unknown, never clear.
    long sky = -1;
    JsonLookup clouds = json_get(props.value, "cloudLayers");
    if (clouds.r == JsonFind::Found && clouds.value != sv("null")) {
        if (!nws_each(clouds.value, [&](sv item) {
            JsonLookup a = json_get(item, "amount"); sv amount;
            if (a.r != JsonFind::Found || !json_str(a.value, &amount)) return false;
            long code = amount == sv("CLR") || amount == sv("SKC") ? 0 : amount == sv("FEW") ? 1 :
                        amount == sv("SCT") || amount == sv("BKN") ? 2 : amount == sv("OVC") ? 3 : -1;
            if (code > sky) sky = code;
            return true;
        })) return make(Err::BadData, "invalid NWS cloud layers");
    }
    f.code = sky;
    int priority = 0;
    JsonLookup weather = json_get(props.value, "presentWeather");
    if (weather.r == JsonFind::Found) {
        if (!nws_each(weather.value, [&](sv item) {
            JsonLookup w = json_get(item, "weather"), in = json_get(item, "intensity"), mo = json_get(item, "modifier");
            sv name, intensity, modifier;
            if (w.r != JsonFind::Found || !json_str(w.value, &name)) return false;
            if (in.r == JsonFind::Found) json_str(in.value, &intensity);
            if (mo.r == JsonFind::Found) json_str(mo.value, &modifier);
            bool heavy = intensity == sv("heavy"), light = intensity == sv("light");
            int rank = 1; long code = -1;
            if (name == sv("thunderstorms")) { rank = 5; code = 95; }
            else if (name == sv("snow") || name == sv("snow_grains") || name == sv("snow_pellets")) { rank = 4; code = light ? 71 : heavy ? 75 : 73; }
            else if (name == sv("rain")) { rank = 3; code = modifier == sv("freezing") ? (heavy ? 67 : 66) : light ? 61 : heavy ? 65 : 63; }
            else if (name == sv("drizzle")) { rank = 3; code = modifier == sv("freezing") ? (heavy ? 57 : 56) : light ? 51 : heavy ? 55 : 53; }
            else if (name == sv("fog") || name == sv("fog_mist")) { rank = 2; code = 45; }
            if (rank > priority) { priority = rank; f.code = code; }
            return true;
        })) return make(Err::BadData, "invalid NWS present weather");
    }
    f.source = "nws";
    f.metric = q.metric;
    *out = f;
    return okay();
}

inline constexpr Provider kNws = {"nws", "api.weather.gov", "443", nws_path, nws_parse, false, 3, nws_next};

} // namespace wx
