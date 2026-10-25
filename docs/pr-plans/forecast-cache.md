# Last-good forecast cache

Status: draft design and acceptance criteria; no implementation yet.

Scope: add a bounded, versioned on-disk forecast cache through a platform storage
seam. Serve matching last-good data when providers fail, visibly marked stale.
Specify location/units identity, provider and observation/fetch timestamps, maximum
age and invalidation rules before coding. Never confuse cache data with live data.

Acceptance:
- Failing host tests first for initial load, successful replacement, restart,
  offline fallback, location/units changes, clock jumps and stale/future timestamps.
- Reject corrupt, truncated, oversized, unsupported-version and wrong-location
  records; mutation-test the new decoder and verify fixed-width/endian-safe format.
- Fault-inject read, short-write, close and replacement failures; no false save
  success, no destruction of the last valid record and no uncontrolled retry loop.
- Keep core/app heap-free. Put file APIs in per-OS adapters. Do not share filenames
  or ownership with TLS seed material.
- Exercise CLI/X11 and OS 9 behavior with fixtures, then verify stale indication
  and recovery on hardware. Run host/32-bit/sanitizer and PowerPC checks.

Dependencies: existing Model and settings seams; coordinate OS 9 persistence with
#12. Provider/geocoding work is not required for this slice.
