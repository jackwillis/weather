# Last-good forecast cache

Set `cache = on` in the config shared by `weather` and `xweather` to enable the
cache. It is off by default. POSIX storage is a separate `<config path>.cache`
file, created privately with mode 0600. Contact details and TLS seeds are never
stored in this file. Plain-HTTP `-H` fixture requests bypass persistent caching.

The entry matches exact latitude, longitude, units and provider selection. Names
are presentation only. A restart may restore an entry fetched at most 24 hours
ago. Future fetch times, corrupt/truncated/oversized files, unknown versions,
unknown sources and invalid forecast fields are rejected. The provider's original
observation text and original fetch time are preserved. Observation text is not
interpreted as a second freshness clock; the age limit applies to fetch time.

The CLI always tries live data first. If that fails and a matching entry exists,
output begins `STALE cached forecast`, and the CLI retains the live failure's exit
status and diagnostic. Cached data therefore never turns a network or certificate
failure into a successful script result. Cancellation does not trigger fallback.
`xweather` may display cached data while trying to update, with a leading
`Cached / stale` marker and dimmed temperature. A live success clears that marker.
Location or unit changes discard the old display and only admit matching entries.
Cached data preserves the five-second manual refresh floor, error/backoff schedule
and unknown-age handling after a clock rollback. A successful live fetch clears
the cached label; rejected manual requests never become queued work.

Writes use an exclusive temporary file in the same directory, complete short
writes, check file sync/close/rename and flush the parent directory. A failure
before rename preserves the old entry; a failure after rename leaves a complete
new entry but reports that persistence failed. Storage failures do not turn a live
forecast into a failure. This is checked replacement, not a physical power-loss
or filesystem durability guarantee. Broken/unreadable cache entries never block
live requests, and no fast retry loop is added.

## Record version 1

Exactly 160 bytes; integers use big-endian order, never native struct layout:

| Offset | Content |
|---|---|
| 0 | Eight bytes `WXFCACH1` |
| 8 | Signed 32-bit latitude and longitude, degrees times 10000 |
| 16 | Metric flag (0/1), source ID (1 Open-Meteo, 2 MET Norway, 3 NWS), six zero bytes |
| 24 | Nonnegative signed-range 64-bit Unix fetch time |
| 32 | Six signed 32-bit fields: temperature tenths, humidity, wind tenths, direction, WMO code, day flag |
| 56 | 24-byte NUL-terminated, zero-padded printable observation text |
| 80 | 64-byte NUL-terminated, zero-padded provider selection |
| 144 | Twelve zero reserved bytes |
| 156 | IEEE CRC32 of the first 156 bytes |

The checksum detects corruption; it does not authenticate a file against someone
who can modify the user's files. The decoder never creates a source pointer into
untrusted input: source IDs map to known static strings. Every accepted record
re-encodes byte-for-byte. Forward extensions must keep rejection behavior safe.

## Current scope

The codec, model policy, POSIX adapter, CLI and X11 implementation are verified.
OS 9 has a cross-built File Manager adapter plus console/window wiring, but its
runtime acceptance is pending a Tiger boot. PR #15 now incorporates main through
2026-10-27, including persistent Preferences, stale-age handling and the manual
refresh floor, and stays draft until the remaining acceptance checks pass.

OS 9 stores `Weather Forecast Cache` in Preferences, separate from all TLS files.
A uniquely named `WXC...` temporary file is created exclusively, written and closed
with checked lengths/results, and volume-flushed before rename/exchange. Replacement,
old temporary deletion and subsequent flushes are checked. The adapter copies file
specifications around mutable Toolbox APIs. Concurrent writers may leave the last
complete entry; no seed lock is reused or stolen. A crash may leave an app-owned
temporary cache file; no automatic secret or lock recovery is performed.

`CacheProbe` is a seed-free, separate diagnostic covering read/write/close/flush/
exchange/rename/delete failures in a fresh disposable folder. `WeatherCacheUITest`
uses a disposable store and explicitly simulated forecasts, then the same live
stage as `WeatherUITest`. Both compile, but have not been run on OS 9 yet. After
building/delivering the diagnostic app, use
`python3 tools/check-os9-window.py OUTPUT --app WeatherCacheUITest`.
Production startup never reads diagnostic marker files or fixture overrides.
Initialized platform adapters carry context pointers; keep them at stable addresses
and do not copy them after initialization.
