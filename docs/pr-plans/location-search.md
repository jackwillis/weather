# Location-name search

Status: draft scope only. Select a geocoding service after checking its current
official documentation, terms, attribution, privacy and rate limits.

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
