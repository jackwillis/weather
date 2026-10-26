# Location search

PR #17 implements the portable foundation and explicit read-only CLI search
(`weather -g "place"`, also `--search`). X11/OS 9 result lists and scripted UI
acceptance are still pending. Normal forecast requests and startup do not search.
The CLI prints numbered matches, region/country and coordinates, with attribution
and a simplification note when needed. It never automatically selects or saves;
use chosen coordinates with -l, an optional -n, and -S in a separate command.
Search rejects -H, -S, -1 and location/units/provider overrides.

## Service and privacy

Selected service: [Open-Meteo geocoding](https://open-meteo.com/en/docs/geocoding-api),
reviewed 2026-10-26. Lookup uses HTTPS at geocoding-api.open-meteo.com/v1/search,
requests five English-language results, and requires an explicit search action.
The query is a place name, not the current coordinates or configuration. No
configured contact is added. Debug traffic logs can contain the query/results.

[Free API terms](https://open-meteo.com/en/terms) restrict use to non-commercial
purposes, below 600 requests/minute, 5000/hour and 10000/day. The service documents
IP logging and troubleshooting logs retained for up to 90 days. Its data license
is CC BY 4.0; attribution for location data is **Open-Meteo / GeoNames**. No
country flag assets are included. Commercial service configuration is outside
this implementation; do not describe the public endpoint as commercially free.

## Bounded behavior

Queries accept 2–63 printable ASCII characters after trimming and are percent
encoded. All production searches use the fixed TLS hostname; testing HTTP
options do not redirect or downgrade them. The caller supplies storage, with
at most five results and a 32768-byte JSON body. Larger, malformed, invalid or
unrepresentable responses fail without changing the caller's results.

Names and administrative regions hold at most 47 ASCII display characters.
Strict UTF-8 and JSON Unicode escapes are decoded; a non-ASCII scalar becomes
one `?`, with a `simplified` flag for UI disclosure. ASCII controls are rejected in
labels; `#` becomes `?` so a result can safely enter the existing config editor.
Long labels fail instead of silently truncating an identity. Country code and
coordinates provide further disambiguation; interfaces must show these and
require explicit selection even for one result. Display names remain editable.

Selection only fills LocationForm. Apply uses checked, atomic Preferences
saving: failure preserves configuration and forecast; success changes location
and discards old location data. Normal coordinate-based use remains available.
No CLI prompt is added, and no first-run network query is automatic.

Search retries at most three times with bounded backoff, supports cancellation,
and propagates TLS, HTTP and transport failures. This is not a global IP quota
manager. Interfaces must submit on an explicit action rather than per keystroke,
serialize searches and avoid automatic repeated queries on service failure.
