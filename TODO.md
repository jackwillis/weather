# TODO

## Now
- [ ] M3c last step: jack runs `xweather` once on maul's real X display (window manager, depth, feel); fix what that shows
- [ ] AddressSanitizer needs `sudo dnf install libasan libubsan` (jack); `make check-san` currently runs UBSan in trap mode only

## Next
- [ ] OS 9 networking gate: Open Transport from Retro68 (headers: Apple's Universal Interfaces from jack, or hand-declared subset + MakeImport stubs); check Classic works on maul's Tiger side (compile gate passed, `make probe-os9`)
- [ ] On-disk stale cache (serve the last good forecast when every source is down)
- [ ] Third source: api.weather.gov (NWS, US only; TLS verified: chains to ISRG X1) needs multi-step requests (points -> stations -> observation) so the Provider interface must grow a step mechanism
- [ ] Location by place name (geocoding), default location prompt
- [ ] SIGHUP config reload / daemon mode (only if the window needs it)

## Packaging (M5, after xweather)
- [ ] app-defaults file for xweather resources (optional)
- [ ] pkgsrc source package (Makefile, DESCR, PLIST, distinfo), verified in a NetBSD VM
- [ ] Verify Open-Meteo terms/attribution; create the GitHub repo and push (jack's call)

## Later
- [ ] Mac OS 9 (Retro68), CI with big-endian check

## Done
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
