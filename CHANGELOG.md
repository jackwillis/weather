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
- A connection that closes before the HTTP status line is complete (an empty reply, or a cut inside `HTTP/1.x ...`) is now a truncated
  answer: retried, exit 74. It was reported as malformed data (exit 65) and not retried, contrary to the man page.
- CLI: `-c` is honoured in every form (`-c FILE`, `-cFILE`, `-Sc FILE`); before, the clustered and attached forms were ignored
  and `-S` rewrote `~/.weatherrc` instead. `-V` and `-h` no longer read the config, so a malformed file cannot block them.
- One fetch attempt is now given up on after three times the `timeout`, counted from its start (`FetchOpts::deadline_s`), and checked before
  every read and write, so the TLS handshake is covered. The timeout applied to each wait, so a server sending a byte every half second could
  hold a fetch for as long as the 64 KB response cap allowed (39 s at `-t 2`; far longer at 1 byte/s). A timeout is retried like any other.
  Name lookup and each connect address are still bounded only by their own waits.
- Tests: compile-time `static_assert`s, 203 unit checks, parser mutation tests, 65 CLI feature tests, a 21-step GUI
  scenario (also on real hardware), UBSan build, vendored and pinned BearSSL, reproducible trust anchors.
