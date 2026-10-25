# RFC: NetBSD/macppc X11 and ctwm desktop UX

**Status:** Draft design proposal · **Date:** 2026-10-25  
**Primary environment:** NetBSD 11/macppc, X11, ctwm, ~1024-pixel desktop  
**Application:** `xweather` (and integration with CLI `weather`)

## Purpose

Give `xweather` a coherent, delightful, usable identity as a **small Unix/X11 desktop application**, with careful behavior under **ctwm** on the Power Mac G4. This is deliberately **not** a Mac OS 9 interface copied to X11, and it does not require CDE, Motif, Xt, GTK or Qt.

The current application already has a 440×300 CDE-inspired window drawn with plain Xlib, a weather illustration, text/status panel, internal File/Options/Help menus, location/about overlays, X resources, and cooperative networking. **Preserve these working foundations.** The following are changes for consideration, not claims that they exist on `main`.

## Context and guiding principles

1. **X11 app, ctwm-managed window.** Let the window manager handle decorations, focus policy, iconification, workspaces/occupations, placement and window actions. Avoid fake title bars, custom window stacking, pointer grabs or homegrown workspace integration.
2. **Quiet utility, fast at a glance.** Condition, temperature, location, and *when the observation was made* must be intelligible immediately. Automatic refresh failures should not steal focus.
3. **The CDE look is a theme, not a dependency.** Keep lightweight drawn widgets unless genuine usability needs justify adding libraries. Use ctwm-friendly geometry and X resources; don't assume CDE services are installed.
4. **Period-appropriate, accessible where possible.** High contrast, visible keyboard focus, readable X core fonts, sufficient hit areas, predictable shortcuts and usable mouse interaction at low resolution and with an old GPU.
5. **Unix-friendly diagnostics.** Prefer concise in-window status, optional log viewing, and an explicit path to CLI diagnostics; do not require an X dialog when the terminal already has full errors.
6. **No hidden blocking or reentrancy.** The cooperative `Net::yield` approach continues; event pumping during a fetch may repaint, update progress, cancel and quit but must not initiate nested requests.
7. **Never disguise stale weather as current.** The last observed timestamp, data source, and freshness are distinct from the last time the app attempted a request.

## Main window

Maintain the **440×300 fixed-size baseline** initially, and explicitly review it on a ~1024×768 (or actual attached monitor) ctwm desktop. The current two-column composition—condition illustration left; location, temperature, conditions and details right—works well. Preserve compactness and low idle CPU/memory.

Suggested hierarchy:
- **Primary:** readable temperature, condition, location.
- **Secondary:** wind and humidity when available; observation timestamp and source.
- **Persistent freshness:** visually distinct `Updated ... ago`, `Fetching...`, `Offline — showing older weather`, or `No weather available`.
- **Actions:** Refresh and Location… (existing buttons), with keyboard access. Additional controls belong in menus, not across the face of the forecast.

Reserve layout space for **stale state even when the error is long**. Avoid truncating `42 min old` after certificate diagnostics. Two-line status could put freshness on a fixed first line and failure/retry message on the second. Do not label an observation time as UTC unless the provider contract actually establishes UTC; Open-Meteo local timestamps and MET Norway UTC timestamps require careful distinction. Do not infer freshness from local fetch time alone.

Potential optional visual enhancement: a small `stale` or disconnected motif in the existing artwork, but not a large new panel. Avoid fast redraws; animated dots, if retained, need bounded refresh cadence.

## Menus and expected keyboard behavior

Keep the existing **File / Options / Help** menu bar and UI nomenclature consistent. Proposed near-term commands:

- **File:** Refresh (F5); Location… (Ctrl+L); Exit (Ctrl+Q).
- **Options:** Auto-refresh (checked); Imperial / Metric (radio); optional Preferences… only once meaningful non-location preferences exist.
- **Help:** About xweather…; optionally Troubleshooting… when written.

A later **View** menu could expose Activity Log… or Connection Details… if those become real windows; do not add empty menus now. A real `Activity Log` window should be a normal ctwm-managed top-level or transient (as appropriate), rather than an enormous overlay inside the 440×300 view.

Keyboard acceptance: Tab and Shift-Tab move within dialogs; Enter accepts and Escape cancels; visibly indicate focus; keyboard menu navigation/escape closing should work predictably; F5/Ctrl+L/Ctrl+Q must remain discoverable. Audit handling of focus-in/focus-out and ctwm focus-follows-mouse configurations. Do not globally grab shortcuts from ctwm.

## Dialogs, errors and diagnostics

**Location:** Existing custom overlay is reasonable if fully keyboard-accessible, with no destructive change on Cancel; validation must name the specific field and preserve the user's selection. A later independent top-level dialog is justified only if focus/menu behavior or sizing demands it.

**Automatic refresh failure:** never interrupt with an attention-grabbing dialog. Keep last-good observation visible and marked stale, show compact status, schedule bounded retry. A manual refresh failure may use an in-window notice instead of a modal popup.

**Terminal diagnostics:** continue structured categories, stderr/syslog and explicit exit statuses in `weather`. For `xweather`, consider a small bounded in-memory recent-events view or a command that explains where persistent logs go. Avoid assuming syslog messages are in a single readable file or that a terminal emulator/browser is installed. Do not expose seed, credential, private TLS or entire HTTP bodies.

**Connection Information (optional):** provider, hostname, last successful observation, last attempt, latest error category and next refresh. Only display details backed by the model; do not claim an unavailable TLS property is known.

## Integration with ctwm and other window managers

- Set conventional `WM_NAME`/icon title and appropriate `WM_CLASS` (for example `xweather` / `Xweather`) so ctwm rules and X resources can target the app. Verify current actual values rather than assuming they are already set.
- Honor `WM_DELETE_WINDOW`, focus events and normal X11 window state; do not call `exit` from network callbacks. Test click-close during fetch and retry.
- Test **iconify/deiconify**, changing ctwm workspaces/occupations, switching focus policies, and moving the window. The app should neither force itself to the foreground nor lose the ability to repaint.
- Allow ctwm to control placement and border/title appearance. Don't fight user `.ctwmrc` preferences or demand specific decorations.
- Inspect X resource precedence and fallback fonts/colors; document `xweather.background`, `.foreground`, `.font`, `.boldFont`, `.bigFont`. Check contrast when background is overridden. Keep plain Xlib and native core fonts for the initial macppc build.
- Consider ICCCM/X11 selection conventions for Copy as a later feature; a custom internal `Copy` button isn't equivalent to CLIPBOARD ownership.
- Fixed size is acceptable for the initial target; enforce sensible WM size hints rather than letting resizing create clipped or unpainted regions. Reconsider resize only after a specific need is demonstrated.
- No tray icon, desktop notification daemon requirement, compositor reliance, GPU acceleration, or Xft/fontconfig dependency in the initial scope.

## Conceptual presentation (not a pixel-perfect mockup)

```text
  +------------------------------------------------------+
  | File   Options   Help                                |
  +------------------------------------------------------+
  |                                                      |
  |  +---------------+  Madison, WI                       |
  |  |               |  54.8 F                            |
  |  |  weather art  |  Clear sky                         |
  |  |               |  Wind 8.7 mph SE · Humidity 84%    |
  |  +---------------+  Observed 04:45 (source/timezone)  |
  |                                                      |
  |  [ Refresh ]   [ Location... ]                       |
  +------------------------------------------------------+
  | Weather from 42 min ago · Open-Meteo                  |
  | Update failed: timed out. Retrying later             |
  +------------------------------------------------------+
```

The schematic shows **information priorities**, not exact geometry or text. Existing window pixels and drawn menu primitives should remain the starting point.

## Implementation slices

### A — Correct status and freshness presentation
- Separate last success / source observation age / last attempt / next retry.
- Guarantee stale age remains visible if an error is long.
- Fix timestamp semantics for both providers and handle wall-clock jumps.
- Host/model tests for failure after success, repeated failure, invalid/future timestamps and no cached data; GUI screenshot checks.

### B — ctwm and keyboard usability
- Audit WM properties, size hints, WM_DELETE_WINDOW, iconify/deiconify, focus events and workspace behavior.
- Verify and improve keyboard menus, tab focus, Enter/Escape, tooltips/help text only if useful.
- Manual test on maul's real display and ctwm, not only Xvfb.

### C — Diagnostics and helpful affordances
- Add clear Help/Troubleshooting content and a small optional modeless Activity Log **if** useful; define in-memory capacity and popup/window-manager behavior first.
- Explore opening or locating a persisted log only after logging policy and implementation are settled; no guessed logfile path.

### D — Packaging and desktop integration
- Include a man page, sample X resources and clear invocation docs.
- Test `pkg_add` install/uninstall on NetBSD/macppc, normal ctwm launch, and an icon/window title if resources support it.
- Keep NetBSD/amd64 and Linux/x86_64 compatible without making their desktop environments design authorities.

## Validation checklist

- Host `make check` and GUI Xvfb suite stay green; no regressions to CLI and OS 9.
- Real Power Mac G4/NetBSD/ctwm visual acceptance is separately recorded, with resolution, fonts, color depth, and screenshots if available.
- Small screen: no cropped menus/dialog actions or illegible freshness indicators.
- Mouse and keyboard: Tab, Shift-Tab, Enter, Escape, F5, Ctrl+L, Ctrl+Q behave consistently.
- Fetch cancellation from close/quit never enters a nested fetch; no window freeze on ordinary network waits beyond documented resolver limitation.
- Status presents stale data honestly, retains usable last-good output and doesn't obscure the age.
- Window remains correct after expose, cover/uncover, iconify/deiconify, focus and ctwm workspace transitions.
- No new mandatory runtime dependencies, and resource use remains appropriate for the 256 MB G4.

## Boundaries and coordination

This RFC is **for NetBSD/macppc, X11 and ctwm first**. The X11 window remains distinct from the classic Mac OS 9 UI (#33). Shared model fixes belong in `src/app` or `src/core`; window-manager and rendering work stays in `xweather`.

Coordinate with existing GUI tests, hardware acceptance (#14), release/packaging (#18), and platform policy (#22). Do not duplicate a broad refactor, require CDE installation, or preempt first-release work.

**Decision requested:** approve the direction and prioritize **A (freshness/status)** and **B (real ctwm usability)** before optional log windows or larger UI additions.
