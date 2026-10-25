# RFC: Native Mac OS 9 user experience and diagnostics

**Status:** Proposed · **Date:** 2026-10-25 · **Target:** Weather on classic Mac OS 9 / PowerPC

## Intent

Make Weather feel like a good classic Macintosh application, not a command-line tool with a window attached. Ordinary use should be quiet and legible; failures should be actionable; detailed diagnostics should be accessible without cluttering the forecast.

This RFC is a design proposal, **not an assertion of existing implementation**. Coordinate with the active native window work (#11), preferences/location work (#12), and follow-up decisions recorded in #23. Avoid opening conflicting code changes before those branches converge.

## Principles

1. **Respect classic Macintosh conventions.** Use real Toolbox menus, windows, controls, alerts, activation, keyboard focus, and appropriate system Help affordances. Refer to period Macintosh HIG documentation during implementation.
2. **Don't interrupt background refreshes.** A routine timeout does not deserve a modal alert. Keep the last good forecast visibly marked stale; show status in the weather window.
3. **Manual actions get actionable feedback.** A failed user-triggered refresh may merit an alert with Try Again or useful recovery guidance. Invalid form fields get specific inline feedback where possible.
4. **Reserve blocking alerts for intervention.** Wrong clock, missing or invalid installation TLS seed, failed secure seed rotation, and storage problems should explain what the user can actually do; no misleading invitation to bypass TLS verification.
5. **Never present dialogs from Open Transport callbacks or inside a nested network event pump.** Queue an error/event, allow the current operation to unwind, and present UI in a safe event-loop state. Bound pending alerts, avoid duplicates, and verify closing/quitting during a fetch.
6. **Keep debugging available, not compulsory.** The forecast view contains weather, age, location, and a compact status. An Activity Log and an optional external diagnostic file offer increasing detail.
7. **Design for limited memory and old displays.** A small bounded log and responsive native controls matter more than animations or a highly abstract widget system.

## Proposed menu affordances

These are **candidate commands**, not a mandate to add empty or disabled menus.

- **Apple menu:** About Weather… according to classic Mac OS conventions, integrated with the system-owned Apple menu.
- **File:** Quit Weather (Command-Q); optionally Save Weather Report… / Print… only after those operations exist.
- **Weather:** Refresh Weather (Command-R); Cancel Refresh when active; Change Location…; Units; Automatic Updates; Preferences…
- **Window:** Weather; Activity Log; optionally Connection Information after its contents have been defined.
- **Help:** Weather Help or Troubleshooting… only when actual help content exists; investigate period Balloon Help integration where available.

Use native enabled/disabled and checked menu states. Do not intercept system-owned menus incorrectly; treat menu tracking during active networking according to the reviewed cooperative-event-loop contract. Menu choices must not cause nested fetches. Apply consistent keyboard conventions for Return, Escape/Command-period and Tab navigation in dialogs.

## Error presentation policy

| Situation | Default UX | Notes |
| --- | --- | --- |
| Successful background refresh | Update the view quietly | Update observation time and provider without popping dialogs |
| Timeout/provider outage during automatic refresh | Short in-window status, stale label and next retry | Preserve last-good data, but never present it as current |
| Manual refresh failure | In-window status; optionally a single alert after operation unwinds | Try Again only when safe; provide Change Location… for location-specific failure |
| Invalid location or preferences | Explain the specific field in the dialog | Do not commit invalid or partial settings |
| Missing/invalid TLS seed, failed rotation, lock conflict | Prominent actionable alert, with recovery/help path | No silent insecure fallback, no secret contents shown |
| Invalid system clock/certificate rejection | Clear security/clock guidance | Do not provide a 'Continue insecurely' button |
| Preferences save failure | Alert that settings were **not** saved | Preserve in-memory state and explain whether changes are active |
| User cancellation | Ordinary status or silence | Not a failure; no retry or duplicate alert |

Keep technical status codes and details in the Activity Log while showing concise human-oriented messages to users. Implement an explicit mapping from internal errors to UX severity and recovery actions; don't expose arbitrary network error strings as button labels.

## Activity Log: modeless diagnostic window

**First version:** an in-memory, bounded, chronological ring buffer; a native modeless Toolbox window reachable under Window > Activity Log. It should open immediately, support scrolling, and remain useful when the disk is read-only or networking is unavailable. Include timestamps, severity, operation/provider, outcome, and optionally error category/code. Offer Copy when reasonably feasible.

Example event types: refresh requested, source selected, connection begun, weather updated, retry scheduled, operation cancelled, configuration save failed, TLS certificate rejected. Avoid dumping whole HTTP bodies, user data, private seed material or secrets. No sensitive seed bytes should enter logs even in debug mode.

**Later, separately scoped:** optional persistent rotating diagnostic text log, explicit size/retention limits, failure handling, an Open Log File menu action, and perhaps Show Log File in Finder. Never assume a particular external editor or file association is present. File operations should follow the classic File Manager/Finder conventions rather than assuming POSIX paths or file formats. Do not force file logging on normal users.

An optional **Connection Information** window may eventually show provider hostname, last refresh, observation timestamp, age, network state, TLS diagnostic category, and retries. Avoid presenting unverified 'TLS is secure' claims.

## Other expected classic Macintosh interactions

- Weather window activation, proper close and quit semantics, moving/resizing only where supported, repainting on update/expose, and normal desktop/window layering.
- Accurate stale/last-updated text always visible even if a long failure message needs truncation; don't let the age/status disappear beyond window bounds.
- Busy cursor and progress cues only when justified; don't monopolize the cursor during a nonblocking background refresh.
- Location/Preferences dialogs with field validation, focus, default and Cancel buttons, Tab navigation, Enter/Escape/Command-period semantics where appropriate; cancel leaves stored preferences unchanged.
- Preserve user choices such as units and auto-refresh; consider window-position persistence independently of weather location data.
- Contextual help / Balloon Help as an optional era-appropriate enhancement.
- Optional notification sound off by default, not an initial requirement.

## Suggested implementation slices

### A. Error UX and event delivery

Define a reusable error classification/presentation mapping and a **non-reentrant** queue/flag handoff from network completion to the main UI event loop. Deliver an alert only when safe; collapse repeated automatic failures and ensure Quit/Cancel works. Add deterministic host tests for state transitions, cooldown/de-duplication, clock faults, and repeated failures. Verify manually under Classic on Power Mac.

### B. Activity Log window

Build a bounded in-memory event history, modeless window, scrolling and menu command. Separate structured event storage from QuickDraw rendering. Test ring wrap, text truncation, long messages, window reopen, refresh during scrolling, low-memory behavior and offline/error conditions. Add persistent file and Finder/open functionality **in a later PR**, once the storage model is agreed.

### C. Interaction and menu polish

Review Apple/File/Weather/Window/Help menus, shortcut dispatch, system-menu ownership, enabling/disabling, keyboard focus, event-loop yielding, alert placement, default button behavior, and status/age readability. Run scripted host tests where possible, inspect screenshots, then require real Macintosh user acceptance. Cross-platform/X11 UX changes are outside this RFC unless a shared portable model defect is uncovered.

## Acceptance and review gates

- No modal alert during periodic failed automatic refresh; stale data remains clearly marked.
- Manual failures show a single comprehensible explanation with applicable recovery.
- No Toolbox dialogs or nested refresh are entered from Open Transport callback/event-pump recursion.
- Quit/cancel, subsequent refresh, window activation, and menu commands work with outstanding network operations.
- Errors and diagnostic events cannot disclose TLS seed material or other secrets.
- Activity Log remains bounded, usable offline, and has tested scrolling, truncation and window lifetime behavior.
- The GUI fits the actual Power Mac display, and logs whether acceptance was in Classic-on-Tiger or native OS 9.
- Existing CLI, X11 and cross-platform build/tests remain green; no regressions to the first OS 9 window and preferences work.

## Non-goals

Not a new generalized widget toolkit or library extraction, not a redesign of weather artwork, not a persistent logging service/daemon, not feature-complete printing/export, not a modern notification-center imitation, and not a reason to delay core security/reliability bug fixes.

## Decision requested

Approve the broad UX direction and prioritize **A: error handling**, **B: Activity Log**, **C: interaction polish** after the base OS 9 window and preferences work reaches a stable integration point. The precise menus and dialogs should be refined with native screenshot review before implementation.
