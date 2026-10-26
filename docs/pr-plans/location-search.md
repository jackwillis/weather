# Location-name search

Status: draft implementation. Portable parser, TLS lookup, explicit-selection
logic and read-only CLI search are implemented; X11/OS 9 controls and runtime UI
acceptance remain pending. Service choice and limits are recorded in `docs/GEOCODING.md`.

Scope: let users search by place name, choose among ambiguous results and save
coordinates/name. Define a first-run location-selection flow without making
unattended CLI use unexpectedly interactive. Preserve explicit coordinate flags.

Acceptance:
- Fixture-test exact/ambiguous/no results, invalid coordinates, oversized responses,
  malformed text, units-independent location identity and service errors.
- Define text encoding/display behavior for X11 and classic Mac text; bound names
  and reject control characters. Mutation-test response parsing.
- Exercise timeout, cancel, retry/rate limiting, selection confirmation and save
  failures. Never change location on a failed search or an unconfirmed result.
- Host tests cover selection state; script X11/OS 9 UI interactions and inspect
  screenshots. Verify persisted selection across restart and old forecast removal.
- No new external service receives a query until the user invokes search; never
  invent a contact or embed the owner's identity in requests.

Dependencies: #12 for OS 9 dialog/persistence integration; coordinate with cache
identity rules. Existing coordinate-only use must keep working.

Portable foundation milestone: percent-encoded 2–63 character explicit queries;
fixed TLS hostname; strict bounded JSON/UTF-8 parsing; five owned results with
coordinates, region and country; ASCII display simplification marked explicitly.
Selection fills editable LocationForm fields without applying configuration.
Tests use the Preferences seam to prove failed saves preserve old location/data
and successful saves reload the chosen coordinates. At that foundation milestone no UI or CLI entry point
invoked search. Remaining work is the original interface/first-run and
hardware acceptance scope above; this milestone does not close the ticket.

Verified milestone: final host, ASan/UBSan and actual NetBSD/macppc unit runs
passed 641 checks. Host/sanitizer robustness passed 231002 inputs; PowerPC
robustness passed 87002 inputs. Existing 99 CLI and 21 GUI checks pass. Retro68
PowerPC/68k compile probes now include the new parser and selection header and
pass. A certificate-verified host lookup of the public place `Madison, WI`
returned four bounded matches. See docs/verification/geocoding-2026-10-26.txt.

Interface integration notes: reject any CLI combination that requests search
with the plain-HTTP fixture option, rather than silently querying production.
Use explicit diagnostic Net injection for scripted fixture searches. X11's
existing apply_location updates RAM before checking settings_save; make it
save-before-apply when integrating selection so the search contract holds there
as it already does in the Preferences tests. Display simplification and service
attribution must be visible beside the result list.

CLI milestone: `weather -g query` / `--search query` lists matches and exits.
It makes no choice and saves nothing; use -l/-n/-S in a separate invocation.
Output credits Open-Meteo / GeoNames and marks simplified names. -g rejects -H,
-S, -1 and location/units/provider overrides before connecting or saving. Queries
are validated before config loading/network initialization. No interactive prompt
was added. A host live TLS lookup passed; offline CLI cases first reproduced
missing help/query validation, then passed with the implementation.

CLI verification: host and ASan/UBSan checks pass (644 unit, 231002 mutation
inputs, 108 CLI feature cases, existing 21 GUI cases). Actual NetBSD/macppc:
644 unit and 87002 mutation inputs pass; the real CLI performed the TLS search
and confirmed configuration unchanged, including a rejected -g/-S combination.
Both portable Retro68 probes pass. Updated manual renders without warnings.
Evidence: docs/verification/geocoding-cli-2026-10-26.txt. No native OS 9 runtime
claim, and no new window-search behavior is claimed at this milestone.
