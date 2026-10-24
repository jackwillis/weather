# TODO

## Now
- [ ] M3c last step: jack runs `xweather` once on maul's real X display (window manager, depth, feel); fix what that shows
- [ ] sanitizer target (`-fsanitize=address,undefined` on the host unit tests) and parser robustness tests

## Next
- [ ] OS 9 gates: Retro68 probe (C++17 core builds, Open Transport usable); check Classic works on maul's Tiger side
- [ ] M4 second provider (check MET Norway / NWS TLS), on-disk stale cache, man page, `make dist` tarball + SHA-512
- [ ] Location by place name (geocoding), default location prompt
- [ ] SIGHUP config reload / daemon mode (only if the window needs it)

## Packaging (M5, after xweather)
- [ ] `make install` (DESTDIR/PREFIX/MANDIR) and `make dist` (tarball + SHA-512); man pages; app-defaults
- [ ] Hand-built NetBSD binary package (pkg_create, or manual tar) from the cross-built install tree
- [ ] pkgin repo on lubulgi (pkg_summary.gz via FTP share); test `pkgin install weather` on maul
- [ ] pkgsrc source package (Makefile, DESCR, PLIST, distinfo), verified in a NetBSD VM
- [ ] Verify Open-Meteo terms/attribution; create the GitHub repo and push (jack's call)

## Later
- [ ] Mac OS 9 (Retro68), CI with big-endian check

## Done
- [x] BearSSL pinned and vendored (third_party/bearssl, commit recorded); built out-of-tree per target; anchors.h reproducible from the PEM with a check (2026-10-24)
- [x] M3c xweather (2026-10-24): CDE-look Xlib window, menus, Location dialog (validation, atomic save), auto-refresh + Refresh button, weather pictures, About; GUI scenario with 21 assertions passes in Xvfb on the laptop and on maul (real PowerPC binary); screenshots reviewed. Not yet seen on a real display.
- [x] M3b seams and model (2026-10-24): Net::yield + Err::Cancelled (SIGINT/SIGTERM cancel a fetch, exit 130), TLS time and entropy from the platform (64-bit clock), portable settings editor + atomic posix store + `weather -S`, app/Model with refresh timers, backoff and stale data. 168 unit checks (also run on maul via `make check-maul`), 65 feature checks (70 online), maul smoke test
- [x] M3a restructure: src/{core,app,tls,platform/posix,cli}, mk/<target>.mk, per-file objects with dep tracking, `make lint` (verified it fails on a planted violation), BearSSL built by the Makefile for each target (2026-10-24)
- [x] M2 core + CLI: errors-as-values, retry/backoff, provider fallback, logging, config, sysexits; 42 unit + 42 feature checks; runs on maul (2026-10-24)
- [x] M1 TLS spike: BearSSL handshake + Open-Meteo fetch works on maul (2026-10-23)
- [x] Cross-toolchain for NetBSD 11.0 macppc, hello + Xlib app run on maul
- [x] C++17 -fno-exceptions -fno-rtti cross-compile check; BearSSL builds for macppc
