# Hardware acceptance

Status: draft; NetBSD main-baseline regression passed, 21-check Xvfb GUI rerun passed.
OS 9 stalled/delayed DNS acceptance and owner real-display review remain open.

Scope: close the remaining DNS delayed-open/stalled-server hardware gaps, rerun
NetBSD/macppc regression tests against merged main, and record the owner's real
display review of xweather. Package reinstall belongs to release readiness.

Acceptance:
- Build only on Linux. Check maul's boot state first; Jack handles required reboots,
  console login, system dialogs and privileged steps.
- Use a controlled delayed resolver fixture to prove actual DNS timeout and
  cancellation, not just a cached result or simulated clock. Do not change global
  network settings without authorization. Keep fixture traffic private to the test.
- Exercise delayed open, late completion, close/retry/shutdown, repeated cancellation,
  no nested fetch, bounded event pumping and resource growth. Record timing and
  callback ownership; distinguish observations from API-contract assumptions.
- Run make check-maul and smoke-maul on NetBSD; run GUI checks there sequentially
  with display :88 unshared. Record exact versions/counts and failing output.
- Jack checks window-manager behavior, depth, fonts and usability on the real X
  display. Do not claim this step from an Xvfb screenshot.
- 68k runtime work stays conditional on an actual target requirement; compilation
  alone is not hardware acceptance.

Dependencies: merged DNS/seed/parser changes; repeat UI-specific checks after #11/#12.

## NetBSD regression, 2026-10-25

Cross-built main baseline 0f97430 on Linux with GCC 12.5.0; executed on maul
(NetBSD 11.0 macppc, G4 7400, 256 MiB). Passed 492 unit checks, 80820 mutation
inputs and all ten smoke checks, including live TLS, missing anchors, future/old
clock rejection, settings preservation, bad host and bad option. Raw result lines
are in docs/verification/netbsd-acceptance-2026-10-25.txt. This baseline excludes
unmerged cache/window/provider fixes; their separate results do not replace it.

No machine configuration or privileged changes were made. The bare sysctl command
was not on the SSH PATH; /sbin/sysctl reported hardware provenance successfully.

The real PowerPC xweather binary also passed all 21 scripted GUI checks in
Xvfb on maul. This covers controls, persistence, cancellation and failure retry;
it does not establish usability on the physical display.
