# NWS observations

Select `providers=nws` or `belike -p nws`. It is opt-in and does not change the
Open-Meteo default. Multiple sources retain the configured fallback order.
Coverage is determined by the NWS point lookup; outside coverage normally returns
HTTP 404. NWS supplies station observations, not a synthesized point forecast.

The chain has exactly three responses:
1. `/points/{latitude},{longitude}` discovers the station-list URL.
2. The validated grid station-list path is requested with `limit=1`.
3. The first listed station's `/observations/latest` provides the observation.

NWS documents the list as ordered by distance from the forecast gridpoint.
We do not search additional stations when the first lacks a usable observation;
that fails this source and permits the next configured provider. Pagination is
not followed. Each response uses the existing fixed 64 KiB caller buffer. Only a
copied relative path survives buffer reuse. A local caller with a smaller buffer
gets a checked response-too-large failure. No heap or third-party JSON tree is
introduced. Arrays are limited to sixteen weather/cloud entries.

Returned links must be unescaped HTTPS URLs with exactly `api.weather.gov`, no
credentials, alternate port (explicit 443 is accepted), fragment, query, percent
encoding, dot segments or doubled slash. Endpoint shape is checked separately.
Every production connection uses the fixed provider host and normal certificate
and hostname verification. The existing explicit `-H` fixture override remains
plain HTTP for tests; link validation still applies. Redirects are not followed.

A User-Agent is always sent. Current NWS policy requires an identifying User-Agent;
contact information is optional. The application sends its actual name/version
and only appends contact when the user configured it. No invented contact or new
trust anchor is used.

Temperature must be Celsius and within -90..60 C before conversion. Wind must
be km/h, humidity percent and direction degrees; supplied values with different
units or impossible ranges fail. Missing/null optional measurements remain unset.
The timestamp must be a valid UTC calendar date from 2025 onwards. Structured
presentWeather and cloudLayers map to WMO conditions; unknown conditions display
as unknown. Day/night remains unset. Observation age is visible in the timestamp;
this provider does not guarantee a station's latest report is recent, and does
not currently reject old observations by age. A fetch timestamp is distinct from
an observation timestamp.

Existing timeouts/retries apply independently to each request; transient failures
retry the current step without repeating completed lookups. There are at most
three steps and at most the configured attempt count per step. This is not an
additional overall wall-clock deadline; the existing blocking POSIX resolver and
network deadline limitations still apply. Cancellation stops the whole chain and
prevents provider fallback. Between steps the event pump gets a cancellation check.

## Official references checked 2026-10-25

- https://www.weather.gov/documentation/services-web-api — request chain, station
  ordering, User-Agent policy, public data and rate limits.
- https://api.weather.gov/openapi.json — gridpoint station-list `limit` (1..500),
  observation fields, quantitative values, METAR weather and cloud enums.

## Verification

The complete host suite, ASan/UBSan and real NetBSD unit/mutation checks pass.
Host and real PowerPC CLI live verified-TLS chains pass. See the PR plan and
docs/verification/nws-2026-10-25.txt for exact counts. The first host live attempt failed
with `response too large` before the station-list limit was added. A separate
public API request confirmed `limit=1` returns one station in a 3777-byte body.
Native OS 9
runtime acceptance awaits Tiger; cross-building is not runtime verification.
The independent cache PR needs an NWS source identifier before these two changes
can persist NWS records together.
