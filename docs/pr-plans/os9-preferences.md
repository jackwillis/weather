# OS 9 location dialog and preferences

Status: draft scope only; no implementation or test results yet.
Depends on the first OS 9 window PR (#11). Keep this PR based on main during
planning; bring in the window through main before implementation.

Scope:
- Native Location dialog using LocationForm/TextEdit validation and predictable
  keyboard focus, Apply/Cancel, and clear validation errors.
- Preferences-folder settings store shared by the OS 9 UI and console. Reuse the
  portable settings editor, retain unrelated settings/comments, and check every
  read/write/close/replacement result.
- Preserve documented weather.conf compatibility with a clear migration and
  precedence rule. Never import or overwrite TLS seed material as preferences.
- Persist location, units and auto-refresh; clear the previous location's forecast
  and request one refresh when a new location is committed.

Acceptance:
- Host tests first for validation/precedence/migration logic and save failure policy.
  Any new parser gets mutation coverage.
- Script OS 9 dialog open, invalid latitude/longitude/name, Apply, Cancel and
  keyboard navigation; inspect screenshots and assert fetch counts.
- Verify restart persistence, missing/read-only/corrupt preferences, failed save
  without false success, and closing/cancelling during a fetch.
- Cross-build on Linux; run host/sanitizer checks and real OS 9 diagnostics.
- No system-dialog interaction or privileged changes on the owner's behalf.
