# Last-good forecast cache

Forecast saving defaults on. Both window apps offer **Save forecasts on this
computer** under Options, corresponding to `cache = on` in the config. Turning it
off removes the saved forecast and prevents subsequent disk reads and writes.
If deletion fails, the menu setting stays on and shows an error; fix the storage
problem before trying again. An externally configured `cache = off` still blocks
reads/writes even when deletion fails, and reports that failure. MET Norway is
skipped with an explanation because it requires response caching. Open-Meteo
remains available without saving. The app does not silently change the configured
provider list. A skipped MET selection does not replace an earlier attempted
provider failure. In-memory forecasts remain available until quitting.

POSIX storage is a separate `<config path>.cache`
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
or filesystem durability guarantee. Broken entries are cache misses; unreadable cache storage skips MET to avoid discarding unknown expiry, while Open-Meteo live requests remain available, and no fast retry loop is added.

## Record version 3

Exactly 224 bytes; integers use big-endian order, never native struct layout:

| Offset | Content |
|---|---|
| 0 | Eight bytes `WXFCACH3` |
| 8 | Signed 32-bit latitude and longitude, degrees times 10000 |
| 16 | Metric flag (0/1), source ID (1 Open-Meteo, 2 MET Norway, 3 NWS), six zero bytes |
| 24 | Nonnegative signed-range 64-bit Unix fetch time |
| 32 | Six signed 32-bit fields: temperature tenths, humidity, wind tenths, direction, WMO code, day flag |
| 56 | 24-byte NUL-terminated, zero-padded printable observation text |
| 80 | 64-byte NUL-terminated, zero-padded provider selection |
| 144 | Eight-byte UTC expiry, then four zero reserved bytes |
| 156 | 64-byte NUL-terminated, zero-padded MET Last-Modified value |
| 220 | IEEE CRC32 of the first 220 bytes |

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

### Native saving-preference acceptance (pending)

On the owner's OS 9 test setup, start with a successful saved forecast, then
relaunch and confirm its age is visible during an update. In Options, uncheck
**Save forecasts on this computer**: the checkmark should clear and the owned
`Weather Forecast Cache` file should disappear. Quit normally and relaunch;
confirm the preference remains off and no disk forecast is restored. With MET
selected, Refresh must explain that forecast saving is required; with Open-Meteo
selected, a successful update must leave no forecast cache file. Turn saving back
on, refresh successfully, and relaunch to confirm restoration works again. Do not
remove or modify any TLS seed or lock file. These steps have not run on hardware.

## MET Norway conditional requests (#128)

The conditional-request branch adds record version 2. Unreleased version-1
records are rejected and replaced after the next live success. The new field
contains only a bounded printable Last-Modified header value, never a timestamp
invented by Weather. Outer HTTP header whitespace is removed; the value itself
is preserved unchanged. Non-MET records must leave this field empty.

Before a MET request, the shared fetch adapter reads a matching, nonexpired
cache record and sends its validator as If-Modified-Since. A 304 is accepted only
with a matching location, units, source and valid prior forecast. It revalidates
the old conditions as a live success; normal save updates the validation time.
A 200 replaces the forecast and validator. An unsolicited 304 cannot manufacture
an empty success. Open-Meteo requests stay unconditional. Plain HTTP fixture
requests bypass disk validators as well as persistence.

MET response caching follows the same saving preference as every provider.
With saving off, MET is skipped before network access, including when a contact
is configured. Diagnostics use only an explicitly supplied cache store; a null
test store disables persistence.
This slice does not establish complete compliance with every MET traffic rule,
including redirects, compression and fleet-wide traffic behavior. Expiry deferral is implemented by #152.

Reference: [MET Norway Terms of Service](https://api.met.no/doc/TermsOfService),
Traffic and Caching sections, checked 2026-10-27.

## Remaining hardware checks for Jack

These are pending on the current integrated branch. Build on Linux only. With
Tiger/Classic running and the owner logged in at the Mac console, configure the
Retro68 build with `WEATHER_BUILD_DIAGNOSTICS=ON` (README), then build with
`make os9`. Quit existing Weather apps normally. From the laptop:

```sh
maulctl push osx tools/deliver-os9.sh deliver-os9.sh
maulctl push osx build/os9/CacheProbe.dsk CacheProbe.dsk
maulctl osx 'sh /tmp/deliver-os9.sh /tmp/CacheProbe.dsk CacheProbe'
maulctl osx 'open /Volumes/MacOS9Lives/weather-dev/CacheProbe'
```

Wait for `cache-probe.log` to end with `finished: 0 failures`, then read it with
`maulctl osx 'cat /Volumes/MacOS9Lives/weather-dev/cache-probe.log'`. Retain any
failure output. Next deliver and run the isolated window diagnostic:

```sh
maulctl push osx build/os9/WeatherCacheUITest.dsk WeatherCacheUITest.dsk
maulctl osx 'sh /tmp/deliver-os9.sh /tmp/WeatherCacheUITest.dsk WeatherCacheUITest'
python3 tools/check-os9-window.py /tmp/weather-cache-shots --app WeatherCacheUITest
```

Inspect the saved screenshots for the stale label, original fetch age and live
recovery. Never remove a TLS seed or live lock, or answer a system dialog as part
of this run. A read-only volume and physical power-loss durability remain
unverified. After Jack boots maul into NetBSD, run `make check-maul smoke-maul`
and `make check-gui-maul` from the laptop, serially; these run cross-built binaries
on maul and need no privileged install. Keep the logs as current-head evidence.

## v3 server expiry and retained MET response (#152)
Each canonical record is still 224 bytes. Bytes 144–151 now contain an unsigned big-endian UTC expiry (zero means no future freshness), bytes 152–155 remain reserved. CRC still covers bytes 0–219. v2 records are discarded as cache misses; settings and TLS seeds are unaffected.

The file holds either one record or two (448 bytes): primary is the latest downloaded forecast, secondary is the matching MET response when primary is another source. New successful MET responses replace both. Open-Meteo success preserves matching MET expiry/validator. Ordinary offline display reads primary; conditional MET requests read the MET record. Reusing a server-valid MET forecast does not rewrite its fetch timestamp or extend its expiry. A 304 updates expiry from its response; missing expiry means revalidate next time. Strict bounded IMF-fixdate parsing rejects malformed/duplicate expiry headers.

Owner acceptance is pending. The CacheProbe commands above exercise the two-record File Manager path after this change. NetBSD's unit suite exercises expiry and source switching with a fake network; run `make check-maul smoke-maul` serially after Jack boots NetBSD. For a production CLI test with no MET network request, stage an isolated config and a synthetic, future-expiry fixture on the laptop:

```sh
mkdir -p /tmp/weather-met152-native
printf 'providers=met-no\ncache=on\nattempts=1\n' > /tmp/weather-met152-native/rc
python3 test/cache_fixture.py /tmp/weather-met152-native/rc.cache
make TARGET=netbsd-macppc all
maulctl push nb build/netbsd-macppc/weather met152-weather
maulctl push nb /tmp/weather-met152-native/rc met152-rc
maulctl push nb /tmp/weather-met152-native/rc.cache met152-rc.cache
maulctl nb '/tmp/met152-weather -v -c /tmp/met152-rc -1'
```

Expect exit zero, `MET Norway: using saved forecast until server expiry`, and `[saved; server-valid]` output; this is synthetic diagnostic data, not live weather. Keep logs. Repeat before fixture expiry; the file must remain unchanged. No personal contact is needed. For Classic, run the seed-free CacheProbe first and retain `finished: 0 failures`.

Live 200/304 behavior and native window labels still need a controlled HTTPS acceptance setup and screenshot review. `-H` deliberately disables persistent cache use, so plain HTTP fixtures cannot prove native expiry behavior. No such setup or native run is claimed here. Inspect source, forecast timestamp and saved/server-valid status in both windows, and saving-off/relaunch behavior, once that acceptance is available. Never run overlapping GUI drivers or touch seed/lock files for these checks.
