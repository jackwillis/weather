# First native OS 9 weather window

Weather now opens a buffered QuickDraw document window using the shared artwork,
forecast model and production network/TLS path. It shows temperature, conditions,
wind, humidity, observation/source and wrapped refresh/error status. Menus provide
About/recovery help, Refresh/Cmd-R, Quit/Cmd-Q, automatic updates and units.
WeatherConsole retains the console diagnostic.

The bounded event pump validates update events and repaints while network calls
yield. During a fetch it suppresses refresh/settings changes and modal menu/drag
tracking; Cmd-Q or the close box cancels. Network initialization happens once,
and terminal shutdown happens only at exit. Rendering restores the previous port,
uses a GWorld when available and falls back to direct drawing if allocation fails.
Seed-store failures are shown with recovery guidance; no lock is stolen or deleted.

Both apps share native, bounded weather.conf loading. Missing files use defaults;
read failures, overlong files and malformed later lines cannot partially change
settings. Units/automatic updates are session-only here; location editing and
Preferences persistence belong to #12. DNS callback event constants are named.

WeatherUITest is a separate optional executable and marker-driven driver. Normal
Weather startup passes no hooks and does not link the fixture/marker source.
Diagnostic screenshots label simulated data explicitly; the live stage uses the
same production fetch function. Numbered acknowledgements avoid repeated stage
advancement and a cross-system marker-deletion race found during verification.

Verification:
- `make check`: 485 unit checks, 186018 robustness inputs, 78 CLI checks,
  21 X11 GUI checks and 2 provisioning tests passed.
- `make check-san`: unit/robustness/CLI passed with ASan and UBSan.
- Retro68 builds Weather, WeatherConsole and optional WeatherUITest; NetBSD/macppc
  test binary cross-compiles. SIZE remains 3 MB preferred / 2 MB minimum and now
  requests suspend/resume events. Production binary contains no test marker or
  fixture-label strings.
- On maul under Classic, the first scripted run captured clear day/night, cloud,
  rain, snow, storm, fog, About, metric, stale error, post-yield and live states.
  Screenshots were inspected locally; no desktop screenshot was published.
- Final pass additionally verified native config reads, partial-change refusal,
  oversized/missing files, offscreen allocation, update-event repaint during a
  yielding fetch, no nested refresh, live certificate-verified TLS, and cancellation
  through the production fetch path. Exactly twelve fetches; zero failures.
  Result log: docs/verification/os9-window-2026-10-25.txt.
- A faster intermediate diagnostic run failed when deleting a shared marker;
  numbered acknowledgements replaced deletion and the final run passed.

Limits: actions are driven programmatically, not a claim of physical mouse/menu
tracking or owner judgment on the real display. Native allocation-failure fallback,
clock-change UI behavior, and actual suspend/resume interaction were not injected
on hardware. NetBSD tests were cross-built, not run on maul while it is in Tiger.
The independent CI (#10) and portable-helper (#13) PRs remain unmerged.
