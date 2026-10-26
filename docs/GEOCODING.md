# Location search

PR #17 implements the portable foundation and explicit read-only CLI search
(`weather -g "place"`, also `--search`). X11 result selection is implemented; OS 9 controls and native OS 9
acceptance remain pending. Normal forecast requests and startup do not search.
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

## X11 window

Location now has **Find...** (also Ctrl-F while editing Location). Enter a query
and press Search/Return; opening the dialog and typing send no requests. Pick a
row using the mouse or arrow keys, then Use/Return to fill Location's editable
fields. Location's OK still saves separately. No row is chosen by default.
Tab/Shift-Tab moves among query, Search, list, Use and Cancel; Ctrl-U clears the
query. Escape/Cancel returns to Location, including during a blocked search;
Ctrl-Q/window close cancels and quits. Changing query text discards old results.
Country and coordinates remain on the second line when a long place/region
label is shortened. Attribution and ASCII simplification are visible.

Location OK now saves before applying. Read/write failure leaves the dialog
open with an explanation, preserving active configuration and current forecast.
The user can repair storage and retry, or cancel. Without an available settings
file, editing fails rather than silently changing only memory. Automatic weather
refresh pauses while Location or search is open, preventing nested requests.

Production HTTP fixture mode refuses place lookup. The GUI test links a separate
`xweather-search-test` with an explicit C++ lookup callback; production `xweather`
contains no fixture generator or environment/marker-based geocoding override.

Supplemental GUI memory checks run the same scripted scenario under ASan/UBSan
with LeakSanitizer disabled; they do not establish UI leak freedom. A first run
trapped before search because the existing status renderer copied from an empty
string view's null data pointer with length zero. A focused test reproduced exit
132. The renderer now avoids null empty-string calls and checks zero/tiny output
capacities before copying or measuring ellipses. The standalone CDE text suite
covers these paths without an X server.

## First window launch

Missing or zero-byte settings open Location before the first weather request.
Existing nonempty configurations retain their startup behavior; this rule does
not reinterpret older configuration files. X11 titles the form "Choose initial
location". Confirming and successfully saving permits the initial weather
update; cancelling exits without saving or fetching. Find remains an explicit
separate search action. Storage errors keep setup pending and preserve the old
configuration. The CLI remains non-interactive and retains its existing defaults.

On OS 9, empty Preferences falls back to the legacy configuration as before;
setup is required only when both sources are empty/missing. Invalid settings
still fail closed and are not treated as a new installation. The fetch gate
remains closed if the native Location resource cannot open. Optional
WeatherFirstApplyTest and WeatherFirstCancelTest diagnostics exercise validation,
failed save, successful save/update, later ordinary Cancel, and first-run Cancel
using memory-only stores and fake forecasts. They read no production Preferences
or TLS seed. Cross-build and actual runtime evidence must be distinguished.

The first-run regression also covers terminal event processing: the test stops
its owned process, queues Cancel then OK, and resumes it. Once Cancel sets quit,
the pump must discard further input; a queued OK must not create settings.
