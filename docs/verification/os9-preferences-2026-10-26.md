# OS 9 Preferences acceptance, 2026-10-26

PR #12 remains draft and unmerged. All compilation occurred on Linux.

## Passing checks

After integration with the merged OS 9 window/current main, `make check` passed:
598 unit checks, 230242 robustness inputs, 106 CLI, 12 local TLS, 21 GUI,
14 anchor checks, six flag checks and provisioning checks. `make check-san`
passed 598 unit, 230242 robustness and 106 CLI checks with ASan/UBSan.
Full Retro68 production and diagnostic builds passed.

On Tiger/Classic, PreferencesProbe first reproduced two failures:

```
FAIL locked file replacement reports failure
FAIL locked file retains original contents
finished: 2 failures
```

File exchange alone allowed replacement of a locked diagnostic file. The adapter
now requires a native write-open and checked close before exchanging an existing
file. The same probe then passed every assertion, including the locked-file
checks, boundary/corrupt-file checks and injected read/write/close/flush/exchange/
rename/remove failures, ending `finished: 0 failures`. Both complete logs are
retained alongside this document as os9-preferences-lock-{red,green}-2026-10-26.txt.
No production Preferences or managed TLS seed files were used by these probes.

## Incomplete native UI runs

The original WeatherPrefsUITest run passed invalid latitude/longitude/name,
save-failure preservation, Apply and exactly one changed-location fetch,
fresh-adapter persisted-field reload, Tab/Shift-Tab and Escape cancellation.
Screenshots of stages 0-2 were inspected locally; the native layout and error
messages were readable. Screenshots remain private local artifacts under
/tmp/weather-12-prefs-20261026.

The original run and an isolated rerun both failed to reach stage 3 readiness:

```
RuntimeError: Preferences stage 3 did not become ready; see saved log
```

Their logs stop after `Escape cancels without changing location or fetching`.
They are incomplete, not passes. System Events subsequently reported that the
WeatherPrefsUITest process did not exist; the cause of the exit is not established.

Additional diagnostic-only checkpoints narrowed the stop: reopening the name-only
editor, setting the name, reading all three native fields, loading the saved
91-byte synthetic record and rewriting/validating its five lines completed.
The last checkpoint was `settings after readback parse`, before the next save
callback. No string-view bounds-abort checkpoint was recorded. This narrows the
investigation but does not establish its cause. Temporary tracing in production
sources was removed; its local patch is /tmp/weather-12-diagnostic-trace.patch.
The incomplete and trace logs are retained beside this document. A marker-only
remote script used the same four acknowledgements without screenshots and timed
out on stage 3 as well; no assertions were disabled.

The next checkpoint build could not be run: maul's Tiger SSH route returned
`No route to host`. The owner was asked to confirm Tiger/console availability.
No force-quit, Classic termination, reboot, system-dialog response or seed recovery
was performed.

## Delivery and remaining acceptance

The production Preferences build including the lock fix was installed at
MacOS9Lives:weather-dev:Weather. The prior production app was preserved with its
resource fork as Weather-before-Preferences. The new production build was not
launched, and no production Preferences migration/save is claimed.

Before readiness: diagnose the repeatable native UI exit; complete the full UI
scenario including name-only Apply and quit cancellation; then verify actual
owner typing and persistence after quitting/relaunching production Weather.
Fresh-adapter readback is not process-restart evidence. Locked-file handling is
verified; a genuinely read-only volume and physical power-loss durability remain
unverified. PR #12 has no merge authorization from this acceptance request.

## Resumed investigation and native fix

Tiger became reachable again without a reboot by the agent. Additional tracing
showed the rewrite returned success and save dispatch was failing. Callback
integrity passed immediately before the Preferences load and failed immediately
after it. The cross-linked binary explained this: the inline writer's 4096-byte
`before` buffer was assigned the same BSS address as subsequent diagnostic globals.
Reading the saved 91-byte record overwrote callback data. This is linked storage
overlap, not a change to the validation rules.

The writer now has one out-of-line definition in src/app/preferences.cpp, giving
its mutable workspace ordinary single-definition storage. Cross-link inspection
shows separate 4096-byte buffers and no overlap with diagnostic globals. All
temporary tracing was removed. The same native marker scenario then completed
with `finished: 0 failures; rc=0`, including name-only Apply, exactly three
scripted fetches and quit cancellation. Full log:
os9-preferences-ui-green-2026-10-26.txt. Earlier incomplete runs remain recorded.
Actual owner typing and production process-restart persistence are still pending.
Delivery of this final production build was not confirmed before the session
ended (agent usage limit); re-deliver and verify before the owner checks. No merge
of #12 is authorized.
