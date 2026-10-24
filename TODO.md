# TODO

## Now
- [ ] M3 Motif-style Xlib window on the core (start here)

## Next
- [ ] M4 second provider (check MET Norway / NWS TLS), on-disk stale cache, man page, `make dist` tarball + SHA-512
- [ ] Pin BearSSL (submodule or vendored) so a fresh clone builds
- [ ] Location by place name (geocoding), default location prompt
- [ ] SIGHUP config reload / daemon mode (only if the window needs it)

## Later
- [ ] Mac OS 9 (Retro68), Tiger, CI with big-endian check

## Done
- [x] M2 core + CLI: errors-as-values, retry/backoff, provider fallback, logging, config, sysexits; 42 unit + 42 feature checks; runs on maul (2026-10-24)
- [x] M1 TLS spike: BearSSL handshake + Open-Meteo fetch works on maul (2026-10-23)
- [x] Cross-toolchain for NetBSD 11.0 macppc, hello + Xlib app run on maul
- [x] C++17 -fno-exceptions -fno-rtti cross-compile check; BearSSL builds for macppc
