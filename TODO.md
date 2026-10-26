# TODO

## Now
- [ ] OS 9 location dialog and Preferences persistence (#12), following the native window (#11)
- [ ] OS 9 hardening acceptance: DNS delayed-open/stalled-server hardware coverage; NetBSD hardware regression after DNS/seed changes (cross-build passes); run under 68k only if a target needs it
- [ ] M3c last step: jack runs `xweather` once on maul's real X display (window manager, depth, feel); fix what that shows
- [ ] Reinstall the rebuilt NetBSD package on maul (`doas pkg_add -u`; staged at /tmp/weather-0.1.0.tgz, lost at reboot: rebuild with tools/mkpkg-netbsd.sh)

## Next
- [ ] UX tier 2 (v0.2): hourly/daily data in the model and providers (#117), pinned locations model and settings (#118); then the UI slices in #98 (OS 9) and #99 (NetBSD). Shared rules: #113. Map size/licence research: #114.
- [ ] On-disk stale cache (serve the last good forecast when every source is down)
- [ ] Third source: api.weather.gov (NWS, US only; TLS verified: chains to ISRG X1) needs multi-step requests (points -> stations -> observation) so the Provider interface must grow a step mechanism
- [ ] Location by place name (geocoding), default location prompt
- [ ] SIGHUP config reload / daemon mode (only if the window needs it)
- [ ] Tests for the OS 9 layer that can run without the Mac (e.g. clock conversion and log format rewrite as pure functions)

## Packaging (M5, after xweather)
- [ ] app-defaults file for xweather resources (optional)
- [ ] pkgsrc source package (Makefile, DESCR, PLIST, distinfo), verified in a NetBSD VM
- [ ] Verify Open-Meteo terms/attribution; create the GitHub repo and push (jack's call)

## Later
- [ ] CI with a big-endian check; Mac OS X Tiger native build (parked)

## Done
- [x] First native OS 9 window (#11): shared art, buffered QuickDraw, menus and model status, bounded yielding event pump, config loading shared with WeatherConsole, separate hardware diagnostic executable. Location editing and Preferences remain follow-up work.
- [x] OS 9 managed TLS seeds (2026-10-25): explicit provisioning, checked rotation, exclusive ownership and HMAC-DRBG; 461 host checks, 186018 mutated inputs, sanitizer/CLI/GUI checks; real-store fault injection on maul; approved genuine seed installation and two live verified-TLS launches with seed advancement. Fixed Retro68 unlink-stub import cleanup through native deletion, with red/green hardware regression.
- [x] OS 9 end to end (2026-10-25): console `Weather` app fetched a live forecast over TLS 1.2 (ECDHE-RSA-ChaCha20-Poly1305, cert verified against our anchors) on a real Mac OS 9.2.2 under Classic. Our own Open Transport glue replaced Apple's (probe 4 proves it); network layer, log sink, SIZE resource, `make os9`. Bugs found on hardware and fixed: event pump spun forever on an unvalidated update event, Retro68 `printf` has no `%zu`/`%lld`, UTC conversion double-counted daylight saving (checked against OS X `date -u`).
- [x] Second data source MET Norway (met-no): one request, worldwide, F/C and m/s conversions, WMO code mapping, `contact` setting + User-Agent (their terms), HARICA root pinned; live-verified through BearSSL; compile-time tests also run on Retro68; fallback and skip-without-contact tested (226 unit, 164k mutated inputs, 78 feature) (2026-10-24)
- [x] OS 9 compile gate (Retro68 GCC 16.1 builds the core + compile-time tests for PowerPC and 68k); xweather resource use measured; ctwm integration scripts tested on copies (2026-10-24)
- [x] Man pages (weather.1, xweather.1, weatherrc.5; groff -mdoc -ww clean), `make install/plist/dist/distcheck` (tarball reproducible, builds and tests from a clean unpack), example config, CHANGELOG (2026-10-24)
- [x] NetBSD binary package via `tools/mkpkg-netbsd.sh`: pkg_create on maul; contents, build info and packing list inspected with pkg_info; install/uninstall test script ready (`runlog pkgtest-weather`), awaiting jack
- [x] `make check-san` (UBSan trap mode + fortify + libstdc++ assertions; ASan automatically once libasan exists) and test/robust.cpp: 108k mutated inputs through every parser with invariants, also run on maul (big-endian); verified it catches a planted off-by-one (26k failures). Makefile fix: test programs now track header changes (a stale robust binary had hidden the planted bug) and objects rebuild when compiler flags change (2026-10-24)
- [x] BearSSL pinned and vendored (third_party/bearssl, commit recorded); built out-of-tree per target; anchors.h reproducible from the PEM with a check (2026-10-24)
- [x] M3c xweather (2026-10-24): CDE-look Xlib window, menus, Location dialog (validation, atomic save), auto-refresh + Refresh button, weather pictures, About; GUI scenario with 21 assertions passes in Xvfb on the laptop and on maul (real PowerPC binary); screenshots reviewed. Not yet seen on a real display.
- [x] M3b seams and model (2026-10-24): Net::yield + Err::Cancelled (SIGINT/SIGTERM cancel a fetch, exit 130), TLS time and entropy from the platform (64-bit clock), portable settings editor + atomic posix store + `weather -S`, app/Model with refresh timers, backoff and stale data. 168 unit checks (also run on maul via `make check-maul`), 65 feature checks (70 online), maul smoke test
- [x] M3a restructure: src/{core,app,tls,platform/posix,cli}, mk/<target>.mk, per-file objects with dep tracking, `make lint` (verified it fails on a planted violation), BearSSL built by the Makefile for each target (2026-10-24)
- [x] M2 core + CLI: errors-as-values, retry/backoff, provider fallback, logging, config, sysexits; 42 unit + 42 feature checks; runs on maul (2026-10-24)
- [x] M1 TLS spike: BearSSL handshake + Open-Meteo fetch works on maul (2026-10-23)
- [x] Cross-toolchain for NetBSD 11.0 macppc, hello + Xlib app run on maul
- [x] C++17 -fno-exceptions -fno-rtti cross-compile check; BearSSL builds for macppc

