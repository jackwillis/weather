# OS 9 Location dialog and Preferences

Status: implemented, draft pending hosted CI, review and native hardware acceptance.
Stacked on #11 (`codex/os9-window`); no prerequisite PR was merged by Codex.

Options > Location (Cmd-L) uses app-owned DLOG/DITL resources and native TextEdit
fields with the shared LocationForm validation. Return applies, Escape/Cmd-. or
Cancel discards, and Tab/Shift-Tab traverses the three fields. Units and automatic
updates have native checkboxes. File Quit remains available. Editing cannot start
during a fetch; automatic updates pause while editing. Save precedes application
of changes, old-location data clears only on a successful coordinate/unit change,
name-only edits keep it, and one refresh is requested. Menu settings use the same
checked transaction. Quit stops event consumption before a queued Apply can run.

Nonempty Preferences:Weather Preferences is the complete source for both native
window and console; otherwise legacy weather.conf, otherwise defaults. Invalid or
unreadable Preferences fails closed. First save preserves legacy text/comments
without modifying weather.conf. Current in-memory values survive failed saves.
Cancel never clears startup configuration errors; after successfully repairing
one, reload the complete saved config before fetching so retained providers are
honored. Ordinary saves do not silently reload unrelated external edits.

The bounded File Manager adapter uses exclusively created, process/tick-named
same-directory temporaries, checked byte counts/close/flush/exchange/rename and
cleanup. A post-replacement error can leave complete new disk contents while
in-memory settings remain unchanged. Physical power-loss durability and concurrent
writer serialization are not claimed. No seed material is migrated or touched.

## Verification

- Host transaction tests cover precedence, whole-source/default semantics,
  migration/comments, invalid coordinates/name, read/save failure, cancellation,
  startup-error recovery retaining provider selection, name-only edits and exactly
  one new-location refresh. Latest unit run: 573 checks passed.
- New mutation coverage exercises selected Preferences text, save/readback and
  output invariants, using the exact persisted byte count. Full host robustness:
  206820 inputs, zero failures on the final run.
- Earlier mutation harness incorrectly reconstructed saved text with strlen and
  reported 20 assertion failures at 67520 inputs for embedded NULs in comments.
  Corrected the harness to retain the actual write length; 67520/0 and then full
  206820/0 passed. No application parser change was used to hide the failure.
- Final host run: 573 unit checks, 206820 mutation inputs, 99 CLI checks, 21 GUI
  checks, anchors/flags/provisioning all pass. ASan and UBSan pass. Actual
  NetBSD/macppc: 573 unit checks, 80820 mutation inputs, zero failures. See
  docs/verification/os9-preferences-2026-10-25.txt for result lines.
- Native Weather, WeatherConsole, PreferencesProbe, WeatherPrefsUITest and the
  updated WeatherUITest cross-build. Portable PPC and 68k checks compile.
- Production binaries contain no diagnostic stage markers or simulated-failure
  text. Existing WeatherUITest now explicitly supplies its own memory-only store.

## Native acceptance still required

Neither new native diagnostic has run yet because maul is currently NetBSD.
PreferencesProbe prepares boundary/corrupt/locked-file and real-adapter fault
injection. WeatherPrefsUITest prepares invalid fields, save failures, Apply/Cancel,
Tab/Shift-Tab/Return/Escape, fresh-adapter readback, preserved forecast/fetch counts,
and Quit during a simulated fetch, with an external marker/screenshot driver.
These are prepared tests, not hardware results. Validate actual typing, dialog
layout, locked/read-only behavior, resource/port lifecycle and real process-restart
persistence on Tiger, then deliver production Weather. A fresh-adapter reload is
not an actual application restart. Instructions: docs/OS9-PREFERENCES.md.

Integration: #15 cache and #16 NWS remain independent branches. Reconcile the
window hooks when combining #15; every diagnostic must supply isolated preference
and legacy stores. CR-only file handling remains the separate #38 change. No
system dialogs, privileged changes, Tiger reboot or PR merge were performed.
