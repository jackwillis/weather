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
| 16 | Metric flag (0/1), source ID (1 Open-Meteo, 2 MET Norway), six zero bytes |
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

The codec, model policy, POSIX adapter, CLI and X11 implementation are present.
OS 9 storage and window integration are still pending; PR #15 remains draft.
The OS 9 window integration follows #11. Provider terms/attribution review remains
part of release readiness; this change makes no new external service contacts.
