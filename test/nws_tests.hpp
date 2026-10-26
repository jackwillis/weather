// Included after the unit harness and fake network helpers.
static Status nws_test_tls(Net &n, Io &raw, const char *host, Io *out)
{
    Fake *f = F(n);
    CHECK(!strcmp(host, "api.weather.gov"));
    if (++f->tls_calls == f->tls_fail_at) return make(Err::Tls, "fixture certificate/hostname rejection");
    *out = raw;
    return okay();
}
static void nws_test_tls_close(Net &, Io &) {}
static bool nws_test_yield(Net &n)
{
    Fake *f = F(n);
    return ++f->yields != f->cancel_yield_at;
}

static void test_nws()
{
    char path[320] = {};
    CHECK(nws_path(Query{430747, -893841, false}, path, sizeof path));
    CHECK(!strcmp(path, "/points/43.0747,-89.3841"));
    CHECK(nws_next(fixture(0, "nws-points.json"), 1, path, sizeof path).ok());
    CHECK(!strcmp(path, "/gridpoints/MKX/38,46/stations?limit=1"));
    CHECK(nws_next(fixture(0, "nws-stations.json"), 2, path, sizeof path).ok());
    CHECK(!strcmp(path, "/stations/KMSN/observations/latest"));
    Forecast f;
    CHECK(nws_parse(fixture(0, "nws-observation.json"), Query{}, &f).ok());
    CHECK(f.temp_t10 == 545 && f.wind_t10 == 112 && f.wind_dir == 270 && f.humidity == 72 && f.code == 2);
    CHECK(!strcmp(f.source, "nws") && !strcmp(f.time, "2026-10-25T23:53") && f.is_day == kUnset);
    CHECK(nws_parse(fixture(0, "nws-observation.json"), Query{0, 0, true}, &f).ok());
    CHECK(f.temp_t10 == 125 && f.wind_t10 == 180 && f.metric);

    const char *hostile[] = {
        "http://api.weather.gov/gridpoints/MKX/38,46/stations",
        "https://api.weather.gov.evil.test/gridpoints/MKX/38,46/stations",
        "https://api.weather.gov@evil.test/gridpoints/MKX/38,46/stations",
        "https://api.weather.gov:444/gridpoints/MKX/38,46/stations",
        "https://api.weather.gov:4430/gridpoints/MKX/38,46/stations",
        "https://evil.test/gridpoints/MKX/38,46/stations",
        "//api.weather.gov/gridpoints/MKX/38,46/stations",
        "https://api.weather.gov//evil.test/stations",
        "https://api.weather.gov/gridpoints/../38,46/stations",
        "https://api.weather.gov/gridpoints/MKX/38,46/stations?redirect=evil",
        "https://api.weather.gov/gridpoints/MKX/38,46/stations#fragment",
        "https://api.weather.gov/gridpoints/%2e%2e/38,46/stations",
        "https://api.weather.gov/gridpoints/MKX/38,46/stations\\evil",
        "https://api.weather.gov/gridpoints/MKX/38,46/stations\r\nX: evil",
    };
    for (const char *url : hostile) {
        sv v;
        CHECK(!nws_url_path(url, &v));
    }
    sv v;
    CHECK(nws_url_path("https://api.weather.gov:443/stations/KMSN", &v) && v == sv("/stations/KMSN"));
    const char *bad_points[] = {"{}", "{", "[]", "{\"properties\":{\"observationStations\":null}}",
        "{\"properties\":{\"observationStations\":\"https://api.weather.gov/alerts/stations\"}}"};
    for (const char *input : bad_points) {
        strcpy(path, "unchanged");
        CHECK(!nws_next(input, 1, path, sizeof path).ok() && !strcmp(path, "unchanged"));
    }
    const char *bad_stations[] = {"{}", "{\"features\":[]}", "{\"features\":[{\"id\":null}]}",
        "{\"features\":[{\"id\":\"https://api.weather.gov/stations/KMSN/observations/latest\"}]}",
        "{\"features\":[{\"id\":\"https://other.example/stations/KMSN\"}]}"};
    for (const char *input : bad_stations) CHECK(!nws_next(input, 2, path, sizeof path).ok());
    CHECK(!nws_next(fixture(0, "nws-points.json"), 0, path, sizeof path).ok());
    CHECK(!nws_next(fixture(0, "nws-points.json"), 1, path, 5).ok());
    CHECK(!nws_path(Query{900001, 0, false}, path, sizeof path));
    CHECK(!nws_path(Query{0, 1800001, false}, path, sizeof path));
    CHECK(!nws_path(Query{}, path, 5));

    // Every failed parse preserves the caller's prior forecast.
    const char *bad_obs[] = {"{}", "{\"properties\":{}}", "{\"properties\":{\"temperature\":{\"value\":null}}}"};
    for (const char *input : bad_obs) {
        f.temp_t10 = 123; f.source = "unchanged";
        CHECK(!nws_parse(input, Query{}, &f).ok() && f.temp_t10 == 123 && !strcmp(f.source, "unchanged"));
    }
    auto changed = [&](const char *from, const char *to) {
        static char altered[2048];
        const char *original = fixture(0, "nws-observation.json");
        const char *at = strstr(original, from);
        CHECK(at != nullptr);
        if (!at) return sv();
        size_t prefix = static_cast<size_t>(at - original);
        snprintf(altered, sizeof altered, "%.*s%s%s", int(prefix), original, to, at + strlen(from));
        return sv(altered);
    };
    CHECK(!nws_parse(changed("12.5", "214748364.7"), Query{}, &f).ok());
    CHECK(!nws_parse(changed("12.5", "60.1"), Query{}, &f).ok());
    CHECK(!nws_parse(changed("degC", "degF"), Query{}, &f).ok());
    CHECK(!nws_parse(changed("km_h-1", "m_s-1"), Query{}, &f).ok());
    CHECK(!nws_parse(changed("72.401374699018", "101"), Query{}, &f).ok());
    CHECK(!nws_parse(changed("270", "361"), Query{}, &f).ok());
    CHECK(!nws_parse(changed("18.0", "-1"), Query{}, &f).ok());
    CHECK(!nws_parse(changed("+00:00", "-05:00"), Query{}, &f).ok());
    CHECK(!nws_parse(changed("2026-10-25", "2026-13-25"), Query{}, &f).ok());
    CHECK(!nws_parse(changed("2026-10-25", "2026-02-30"), Query{}, &f).ok());
    CHECK(!nws_parse(changed("23:53:00", "24:53:00"), Query{}, &f).ok());
    CHECK(nws_parse(changed("18.0", "null"), Query{}, &f).ok() && f.wind_t10 == kUnset);
    CHECK(nws_parse(changed("\"BKN\"", "\"CLR\""), Query{}, &f).ok() && f.code == 0);
    CHECK(nws_parse(changed("\"BKN\"", "\"OVC\""), Query{}, &f).ok() && f.code == 3);
    CHECK(nws_parse(changed("\"BKN\"", "\"VV\""), Query{}, &f).ok() && f.code == -1);
    struct WeatherCase { const char *items; long code; };
    const WeatherCase weather[] = {
        {"[{\"weather\":\"rain\",\"intensity\":\"light\"}]", 61},
        {"[{\"weather\":\"rain\",\"intensity\":\"heavy\",\"modifier\":\"freezing\"}]", 67},
        {"[{\"weather\":\"snow\",\"intensity\":\"heavy\"}]", 75},
        {"[{\"weather\":\"drizzle\"}]", 53},
        {"[{\"weather\":\"fog\"}]", 45},
        {"[{\"weather\":\"rain\"},{\"weather\":\"thunderstorms\"}]", 95},
        {"[{\"weather\":\"unknown\"}]", -1},
    };
    for (const WeatherCase &w : weather) {
        CHECK(nws_parse(changed("[]", w.items), Query{}, &f).ok() && f.code == w.code);
    }
    // Full chain, bounded failures and cancellation at every transition.
    static char responses[3][2048];
    const char *fixtures[] = {"nws-points.json", "nws-stations.json", "nws-observation.json"};
    for (int i = 0; i < 3; ++i) snprintf(responses[i], sizeof responses[i], "HTTP/1.0 200 OK\r\n\r\n%s", fixture(0, fixtures[i]));
    const Provider *only[] = {&kNws}, *fallback[] = {&kNws, &kOpenMeteo};
    FetchOpts opts; opts.attempts = 1; opts.force_host = "fixture"; opts.force_port = "80";
    char buf[4096];
    auto chain = [&]() {
        Fake fake;
        fake.nsteps = 3;
        for (int i = 0; i < 3; ++i) fake.steps[i] = {okay(), responses[i], Err::Ok};
        return fake;
    };
    {
        Fake fake = chain(); Net net = make_net(&fake);
        CHECK(fetch_forecast(net, only, 1, Query{}, opts, buf, sizeof buf, &f).ok());
        CHECK(fake.connects == 3 && fake.closes == 3 && f.temp_t10 == 545);
        CHECK(strstr(fake.request, "GET /stations/KMSN/observations/latest HTTP/1.0") != nullptr);
    }
    {
        Fake fake = chain(); Net net = make_net(&fake);
        net.tls_open = nws_test_tls; net.tls_close = nws_test_tls_close;
        FetchOpts live; live.attempts = 1;
        CHECK(fetch_forecast(net, only, 1, Query{}, live, buf, sizeof buf, &f).ok());
        CHECK(fake.tls_calls == 3 && fake.closes == 3);
    }
    for (int step = 0; step < 3; ++step) {
        for (const char *retry_status : {k500, "HTTP/1.0 429 Too Many Requests\r\n\r\n"}) {
            Fake retry; retry.nsteps = 4;
            for (int i = 0; i < step; ++i) retry.steps[i] = {okay(), responses[i], Err::Ok};
            retry.steps[step] = {okay(), retry_status, Err::Ok};
            for (int i = step; i < 3; ++i) retry.steps[i + 1] = {okay(), responses[i], Err::Ok};
            Net net = make_net(&retry); FetchOpts retried = opts; retried.attempts = 2; retried.backoff_ms = 1;
            CHECK(fetch_forecast(net, only, 1, Query{}, retried, buf, sizeof buf, &f).ok());
            CHECK(retry.connects == 4 && retry.closes == 4 && retry.nsleeps == 1);
        }
        for (Err err : {Err::Cancelled, Err::Timeout}) {
            Fake fake = chain(); fake.steps[step].connect_result = make(err, "fixture stop");
            Net net = make_net(&fake);
            CHECK(fetch_forecast(net, err == Err::Cancelled ? fallback : only, err == Err::Cancelled ? 2 : 1,
                                 Query{}, opts, buf, sizeof buf, &f).err == err);
            CHECK(fake.connects == step + 1 && fake.closes == step);
        }
        Fake fake = chain(); Net net = make_net(&fake);
        net.yield = nws_test_yield; fake.cancel_yield_at = step + 1;
        CHECK(fetch_forecast(net, fallback, 2, Query{}, opts, buf, sizeof buf, &f).err == Err::Cancelled);
        CHECK(fake.connects == step && fake.closes == step);
        Fake cert = chain(); net = make_net(&cert);
        net.tls_open = nws_test_tls; net.tls_close = nws_test_tls_close; cert.tls_fail_at = step + 1;
        FetchOpts live; live.attempts = 1;
        CHECK(fetch_forecast(net, only, 1, Query{}, live, buf, sizeof buf, &f).err == Err::Tls);
        CHECK(cert.connects == step + 1 && cert.closes == step + 1 && !strcmp(cert.last_host, "api.weather.gov"));
        const char *statuses[] = {k404, "HTTP/1.0 429 Too Many Requests\r\n\r\n", k500};
        for (const char *response : statuses) {
            Fake failed = chain(); failed.steps[step].response = response; net = make_net(&failed);
            CHECK(fetch_forecast(net, only, 1, Query{}, opts, buf, sizeof buf, &f).err == Err::HttpStatus);
            CHECK(failed.connects == step + 1 && failed.closes == step + 1);
        }
    }
    {
        Fake fake = chain(); fake.steps[0].response = "HTTP/1.0 200 OK\r\n\r\n{}";
        fake.steps[1].response = kGood;
        Net net = make_net(&fake);
        CHECK(fetch_forecast(net, fallback, 2, Query{}, opts, buf, sizeof buf, &f).ok() && !strcmp(f.source, "open-meteo"));
        CHECK(fake.connects == 2);
    }
    {
        Provider invalid = kNws; invalid.steps = 4; const Provider *bad[] = {&invalid};
        Fake fake = chain(); Net net = make_net(&fake);
        CHECK(fetch_forecast(net, bad, 1, Query{}, opts, buf, sizeof buf, &f).err == Err::BadData && fake.connects == 0);
    }
}
