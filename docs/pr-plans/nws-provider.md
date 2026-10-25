# NWS provider

Status: draft scope; verify current official API documentation before implementation.

Scope: extend the provider/fetch seam for bounded multi-step requests and add
api.weather.gov as a US-only source. Preserve current Open-Meteo/MET Norway
behavior, error values, fallback and cancellation semantics. Define step count,
buffer ownership, station-selection policy and missing-observation behavior first.

Acceptance:
- Use official API policy/docs when implementing; verify contact/User-Agent,
  attribution and usage requirements. Never invent contact details.
- Fixture-test the full request chain, coordinates outside coverage, missing fields,
  station lists, no observations, units and condition mapping, 404/429/5xx responses,
  malformed data and certificate/hostname failures.
- Test cancellation/deadlines at each step, no fallback after cancellation, bounded
  loops and response buffers, and compatibility of both existing providers.
- Mutation coverage for every new parser path; host, 32-bit, sanitizers and real
  PowerPC validation. Live verification is separate from deterministic fixture CI.
- Review any trust-anchor changes separately; never bypass TLS verification or
  trust arbitrary hosts returned by an API without a documented policy.

Dependencies: provider interface design; caching can remain independent. No daemon
or geocoding requirement is introduced by this PR.
