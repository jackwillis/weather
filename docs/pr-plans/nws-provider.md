# NWS provider

Status: implemented and locally verified, draft pending hosted CI and review. This PR adds an
opt-in `nws` source without changing the default or either existing provider.

The provider seam supports one to three responses. NWS uses points -> gridpoint
stations with limit=1 -> first station's latest observation. Each step copies only
a validated relative path before reusing the caller's fixed response buffer.
The host remains api.weather.gov with normal TLS/hostname verification, and the
next source is tried if the first station has no usable observation. No station
pagination/search or redirects are followed. Contact is optional under the current
NWS policy; the actual application User-Agent is always present.

All followed links are constrained to HTTPS api.weather.gov (optional explicit
443), approved endpoint shapes and a narrow path alphabet. Credentials, foreign
hosts, other ports, escaped URLs, query/fragment/dot segments and doubled slashes
are rejected. The only generated query is the station-list limit.

Temperature/optional measurements validate units and bounds before conversion.
UTC timestamps validate calendar ranges. Conditions use structured NWS METAR
weather/cloud enums, with explicit unknown conditions and unset day/night.
The latest observation may be older than the fetch time; no station observation
age cutoff is currently implemented. See docs/NWS.md for detailed scope/policy.

## Tests and verification

- Tests added before implementation; timestamp/date regressions then failed three
  checks before calendar validation, and the limit regression failed before the
  generated station-list limit was added.
- 651 host unit checks and 299168 mutation inputs passed. New mutations exercise
  every observation, point/station link and URL parser; failures preserve outputs.
- Real CLI fixtures exercise the full three-request chain in both units and
  outside-coverage 404 behavior. 104 feature checks passed.
- Fake-network tests cover cancellation/yield between every step, TLS rejection,
  404/429/500 at every step, retrying only the current step, and provider fallback.
  TLS tests here test error propagation; they are not an independent certificate
  verifier implementation or a substitute for the existing TLS tests.
- Actual NetBSD/macppc: 651 unit checks and 101168 mutation inputs passed.
- Portable PPC and 68k compilation and full OS 9 application cross-build passed.
  No new OS 9 runtime claim: Tiger is still required.
- Initial live host run failed with `response too large`. Official API schema
  inspection and a public limit=1 request confirmed a 3777-byte one-station body;
  the generated limit fixed the live chain. A subsequent host verified-TLS run
  returned 60.8F clear sky, source nws, observation 2026-10-26T00:15.
- Final host check: all 651 unit, 299168 mutation, 104 CLI and 21 GUI checks
  passed, plus anchors/flags/provisioning. ASan and UBSan passed. The native
  PowerPC CLI independently completed the live verified-TLS NWS chain and
  returned the same observation. Result lines are preserved in
  docs/verification/nws-2026-10-25.txt. Existing GUI checks cover UI regression,
  not a scripted native NWS window fetch.

No trust anchor, privileged machine configuration, API contact address or Apple
material was added. The independently pending cache PR needs an NWS source ID
before it can persist this source. The separate whole-attempt deadline PR #41
needs integration review around the three-step loop and OS 9 active-time clock.

Official docs checked 2026-10-25:
https://www.weather.gov/documentation/services-web-api
https://api.weather.gov/openapi.json
