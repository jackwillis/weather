# Changelog

## 0.1.0 (unreleased)
First version.
- `weather`: current conditions over TLS (built-in BearSSL, no proxy) from Open-Meteo; retries with backoff,
  data-source fallback, sysexits exit codes, settings file (`~/.weatherrc`), `-S` to save the location,
  clean cancel on SIGINT/SIGTERM.
- `xweather`: CDE-look X11 window with menus, Location dialog, weather pictures, auto-refresh every 15 minutes,
  stale-data display after failures.
- Portable core (`src/core`, `src/app`) with an OS seam; NetBSD/macppc is the first target.
- Second data source `met-no` (MET Norway; needs `contact` in the config because their terms require it).
- Mac OS 9 (first cut): `make os9` builds a PowerPC console app `Weather` with Retro68. It uses Open Transport through our own declarations
  and client glue (no Apple files), the same TLS/HTTP/JSON/core code, and fetched a live forecast on a real Mac OS 9.2.2.
- CLI: `-c` is honoured in every form (`-c FILE`, `-cFILE`, `-Sc FILE`); before, the clustered and attached forms were ignored
  and `-S` rewrote `~/.weatherrc` instead. `-V` and `-h` no longer read the config, so a malformed file cannot block them.
- Tests: compile-time `static_assert`s, 203 unit checks, parser mutation tests, 65 CLI feature tests, a 21-step GUI
  scenario (also on real hardware), UBSan build, vendored and pinned BearSSL, reproducible trust anchors.
