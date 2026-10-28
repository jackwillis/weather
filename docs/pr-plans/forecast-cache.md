# Last-good forecast cache

Status: draft implementation incorporating main through 2026-10-27. Portable/POSIX milestones are verified;
OS 9 storage and window/console integration are cross-built, with native runtime
acceptance pending a Tiger boot.

Implemented:
- Canonical 160-byte big-endian record with CRC, known source IDs, bounded fields,
  exact location/units/provider identity and a 24-hour restore limit. Future fetch
  times and corrupt/oversized/unknown records are rejected. Decoder source pointers
  refer to static strings; accepted records re-encode byte-for-byte.
- Opt-in `cache=on`, separate private `<config>.cache`; no contact or TLS seed data.
  Plain-HTTP fixture requests bypass persistence. CLI cached output is explicitly
  STALE and retains the live failure exit code; cancellation does not fall back.
- Model/X11 cache restoration, stale-first status, dimmed temperature, live-success
  clearing, restart and unit/location invalidation. No new retry loop.
- Checked POSIX read/atomic write seam; native tests inject read, partial write,
  file flush/close, rename, directory flush/close failures. Before replacement the
  old valid record remains; after replacement failure is still reported but the
  complete new record remains. Temporary files are cleaned up.

Verification:
- Host: 923 unit/storage checks, 230242 robustness inputs, 99 existing CLI checks,
  4 new end-to-end CLI cache tests, 6 flags tests, 14 anchor tests, 2 provisioning
  tests; ASan and UBSan passed. New decoder mutations also recompute CRC to reach
  numeric/text/canonical validation beyond checksum rejection.
- X11 GUI: 30 checks passed on host and on actual NetBSD/macppc. Native cache,
  restart and changed-unit screenshots were inspected locally. A first native
  restart run failed to show the second window; private Xvfb was hardened with
  `-noreset` and remote folders were isolated, and the rerun passed. The original
  startup failure's exact cause was not proven; failure logs are now retained.
- Actual PowerPC: 923 unit checks, 86242 mutation inputs (the hardware driver uses
  fewer random iterations), all ten smoke checks passed. Linux cross-built only.
- Native live CLI created a 160-byte cache; deliberate trust-anchor rejection
  returned 76 and printed STALE data. Disposable test files were removed. Log:
  docs/verification/cache-netbsd-live-2026-10-25.txt.
- Portable code compiles with Retro68 for both PowerPC and 68k. OS 9 adapter and
  GUI behavior are not claimed tested. GitHub's 32-bit job runs on this PR.

See docs/FORECAST-CACHE.md for the record and policy. Remaining before ready:
Native OS 9 storage-fault and cache-window acceptance when Jack boots Tiger.
The window integration now incorporates main, including #11 and #12. Keep draft.

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

Combined-tree follow-up (after bringing in #11 at 8086781): host `make check` and
ASan/UBSan pass with 968 unit checks, 230242 mutation inputs and 30 GUI checks.
Actual NetBSD/macppc unit/robustness passed with 968 checks / 86242 inputs. Portable
code compiles for Retro68 PowerPC and 68k; Weather, WeatherConsole, CacheProbe,
WeatherUITest and WeatherCacheUITest cross-build successfully. No new native OS 9
runtime claim: maul is booted into NetBSD. Production Weather contains no marker
or fixture-label strings. The same adapter instance has an explicit native-probe
replacement/read regression ready to run; probe data stays in owned test folders.

Cached clock-rollback status now says its fetch time is ahead of the clock instead
of `just now`; age conversion clamps huge forward jumps safely on 32-bit long.
The initial failing rollback test was reproduced. A concurrent header edit left
one stale host test object; affected objects were forced to rebuild and the full
combined suite passed. No source edits overlapped the final verification builds.

Merge interaction: #35 also changes Model age/backward-clock semantics; preserve
its live-data behavior together with these cached-status/overflow guards. #41's
wall-clock total deadline also needs to respect #11's modal active-time decision.

NWS integration: cache source ID 3 is reserved for `nws`; IDs 1/2 and the
160-byte format remain unchanged. Only NWS may store condition code -1 (explicit
unknown conditions). Unknown provider IDs and other invalid condition codes
remain rejected. Unit cases cover known/unknown NWS round trips and rejection
for the other providers. Mutation coverage includes NWS records, with repaired
CRCs to reach field validation. A temporary combined-header check parses the
#16 NWS observation fixture and round-trips both known and unknown conditions;
this is a host integration check, not a native OS 9 runtime claim.

Verification of this bridge: host 978 unit checks, 30 GUI checks, 99 CLI checks
passed; sanitizer run explicitly enabled ASan and passed 978 unit checks and
253444 mutation inputs. Actual NetBSD/macppc passed 978 unit checks and 91444
mutation inputs. Both Retro68 portable compiler probes passed. Initial new NWS
unit cases failed before the codec change; the sandbox also blocked an unrelated
local socket test, which passed in the network-enabled regression run. The full
host check preceded the added NWS mutation seed; the sanitizer and NetBSD runs
include that seed.

2026-10-27 main integration: see ../verification/cache-main-integration-2026-10-27.txt
for current host/sanitizer/cross-build results and tests held for fixture isolation.
