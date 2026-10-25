# OS 9 window: first usable slice

Status: draft scope and acceptance criteria; implementation has not started.

Replace the console as the normal OS 9 entry point with a native document window,
while retaining the console build for diagnostics. Reuse Model, the shared weather
art and the existing fetch/TLS pipeline. Keep the first slice separate from
location editing and Preferences persistence.

Implementation:
- A compact Platinum-style window, QuickDraw canvas and readable temperature,
  conditions, wind, humidity, observation time and model status.
- Apple/About, File/Refresh/Quit and Options/units/auto-refresh menus.
- A bounded WaitNextEvent loop; validate update events, handle activation and
  suspend/resume, allow close/quit to cancel fetch, and suppress nested refreshes.
- Share config loading with the console; preserve weather.conf behavior.
- A separate scripted diagnostic mode that drives app actions without clicking
  system dialogs. Production startup must not accidentally enable test overrides.

Acceptance:
- Cross-build on Linux; never compile on maul. Preserve the 3 MB SIZE resource.
- Host tests cover any newly extracted state decisions; new input parsers get
  mutation coverage. Run host and sanitizer regressions.
- On maul, script initial fetch, redraw, refresh, auto-refresh/units and close during
  fetch. Assert no nested fetch and no endless update-event loop.
- Inspect screenshots for clear day/night, cloud, rain, snow, storm, fog and error
  states. Distinguish observed hardware behavior from simulated conditions.
- Keep existing CLI/X11 behavior green and document the new OS 9 launch workflow.

Dependencies: existing merged DNS/seed/artwork work. CI PR #10 is separate.
Follow-up: location dialog and persistent preferences in their own PR.
