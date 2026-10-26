# Location-name search

Status: draft implementation. Portable parser, TLS lookup and explicit-selection
logic are implemented; CLI/X11/OS 9 entry points and runtime UI acceptance remain
pending. Service choice and limits are recorded in `docs/GEOCODING.md`.

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
and successful saves reload the chosen coordinates. No UI or CLI entry point
currently invokes search. Remaining work is the original interface/first-run and
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
