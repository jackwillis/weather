# Changelog

## 0.1.0 (unreleased)
First version.
- Both windows ignore manual refresh requests within five seconds of the last fetch starting; automatic refresh and location changes remain immediate.
- `weather`: current conditions over TLS (built-in BearSSL, no proxy) from Open-Meteo; retries with backoff,
  data-source fallback, sysexits exit codes, settings file (`~/.weatherrc`), `-S` to save the location,
  clean cancel on SIGINT/SIGTERM.
- `xweather`: CDE-look X11 window with menus, Location dialog, weather pictures, auto-refresh every 15 minutes,
  stale-data display after failures.
- Portable core (`src/core`, `src/app`) with an OS seam; NetBSD/macppc is the first target.
- Second data source `met-no` (MET Norway; needs `contact` in the config because their terms require it).
- Mac OS 9 (first cut): `make os9` builds a PowerPC console app `Weather` with Retro68. It uses Open Transport through our own declarations
  and client glue (no Apple files), the same TLS/HTTP/JSON/core code, and fetched a live forecast on a real Mac OS 9.2.2.
- A connection that closes before the HTTP status line is complete (an empty reply, or a cut inside `HTTP/1.x ...`) is now a truncated
  answer: retried, exit 74. It was reported as malformed data (exit 65) and not retried, contrary to the man page.
- CLI: `-c` is honoured in every form (`-c FILE`, `-cFILE`, `-Sc FILE`); before, the clustered and attached forms were ignored
  and `-S` rewrote `~/.weatherrc` instead. `-V` and `-h` no longer read the config, so a malformed file cannot block them.
- Retry delays still start at 500 ms and double, but never exceed 30 seconds (`FetchOpts::max_backoff_ms`). Before, a large `attempts`
  (up to 300) meant sleeps of hours, and past 22 doublings the delay overflowed a 32-bit `long`.
- The config parser and `-S` accept LF, CRLF or a lone CR (classic Mac) as line breaks. A CR-only file used to be read as one line (a comment,
  if it began with `#`) and silently gave the default location; `-S` now edits such files in place and keeps their line ending. The OS 9
  console app's own `weather.conf` reader is unchanged.
- If the system clock is stepped back past the moment the forecast was fetched, the status line now says the data is from "an unknown
  time, clock changed" and the data counts as stale; before, it kept saying "Updated just now".
- A provider answer with an impossible humidity (outside 0-100 %), wind speed, wind direction, `is_day` or weather code is refused, so the next
  source is tried; before, it was shown. Open-Meteo answers whose `current_units` differ from the requested units are refused too (the
  labels come from the request). MET Norway values are range-checked before unit conversion, which also removes a 32-bit overflow.
- Tests: compile-time `static_assert`s, 203 unit checks, parser mutation tests, 65 CLI feature tests, a 21-step GUI
  scenario (also on real hardware), UBSan build, vendored and pinned BearSSL, reproducible trust anchors.
