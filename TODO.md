# TODO

## Now
- [ ] M3a restructure into src/core, app, tls, platform/posix, cli; mk/<target>.mk; `make lint` (portable-core check)
- [ ] M3b seams: Net::yield (+ Err::Cancelled), Net::entropy, TLS time/entropy from the platform; portable settings
      text update (keep comments) + tests; app/Model (refresh timers, stale data, backoff) + tests with fake clock
- [ ] M3c xweather: CDE-look Xlib window using the Model (auto-refresh 15 min + Refresh button, location dialog)
- [ ] `make check-maul` (cross-built unit tests run on maul = big-endian proof); sanitizer target; pin BearSSL

## Next
- [ ] Compiler gates before ports: Retro68 probe (C++17, Open Transport); Tiger modern cross-GCC feasibility
- [ ] M4 second provider (check MET Norway / NWS TLS), on-disk stale cache, man page, `make dist` tarball + SHA-512
- [ ] Pin BearSSL (submodule or vendored) so a fresh clone builds
- [ ] Location by place name (geocoding), default location prompt
- [ ] SIGHUP config reload / daemon mode (only if the window needs it)

## Packaging (M5, after xweather)
- [ ] `make install` (DESTDIR/PREFIX/MANDIR) and `make dist` (tarball + SHA-512); man pages; app-defaults
- [ ] Hand-built NetBSD binary package (pkg_create, or manual tar) from the cross-built install tree
- [ ] pkgin repo on lubulgi (pkg_summary.gz via FTP share); test `pkgin install weather` on maul
- [ ] pkgsrc source package (Makefile, DESCR, PLIST, distinfo), verified in a NetBSD VM
- [ ] Verify Open-Meteo terms/attribution; create the GitHub repo and push (jack's call)

## Later
- [ ] Mac OS 9 (Retro68), Tiger, CI with big-endian check

## Done
- [x] M2 core + CLI: errors-as-values, retry/backoff, provider fallback, logging, config, sysexits; 42 unit + 42 feature checks; runs on maul (2026-10-24)
- [x] M1 TLS spike: BearSSL handshake + Open-Meteo fetch works on maul (2026-10-23)
- [x] Cross-toolchain for NetBSD 11.0 macppc, hello + Xlib app run on maul
- [x] C++17 -fno-exceptions -fno-rtti cross-compile check; BearSSL builds for macppc
