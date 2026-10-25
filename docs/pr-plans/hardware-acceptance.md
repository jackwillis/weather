# Hardware acceptance

Status: draft verification plan; no new results are claimed.

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
