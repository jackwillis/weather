# OS 9 window: decisions and follow-up work (2026-10-25)

Status: **planned, not implemented.** Nothing here describes behavior on `main`. It records what the owner decided after
review of the native window PR (#11) so the next implementer does not have to rediscover it, and it lists the documentation
that must change together with the code. Other PRs: #10 (CI) and #13 (OS 9 helpers) were approved in review and are not covered here.

## Decisions (owner, 2026-10-25)

1. **Background execution.** The SIZE resource is `cannotBackground`, so the app gets no time while another application is in
   front. Under Classic on Tiger that happens whenever an OS X window is clicked, so a fetch in progress probably stalls and times
   out on return, and automatic refresh pauses. Change the flag to `canBackground`, handle `osEvt` properly (check the
   suspend/resume bit instead of redrawing on every `osEvt`), and test by clicking away from the window mid-fetch on maul.
   The "probably stalls" is a reading of the flag's documentation, not an observed result; confirm it on hardware first.
2. **Menu bar during a fetch.** Today, clicks in the menu bar are ignored while a fetch runs, including the system Application
   menu, and dragging is disabled, so File > Quit looks enabled but cannot be clicked. Allow menu tracking and dragging instead.
   Because `MenuSelect` and `DragWindow` run their own event loops, the network yield cannot run while they track, so deadlines
   must not count that time:
   * Keep one counter of ticks spent in modal tracking.
   * Both deadline clocks subtract it: `mono_ms()` and `DnsBackend::now()` in `src/platform/os9/net_os9.cpp`, which both use
     `TickCount()`. These are the only two deadline clocks found by grepping `src/platform/os9`, `src/core/fetch.cpp` and `src/tls` at #11 head 9b27a5c; re-check before relying on it.
   * The event pump measures the tick count around `MenuSelect` and `DragWindow` and adds the elapsed time to the counter.
   * Remove the `!fetching` guards on menu-bar and drag mouse-downs.
   Arithmetic must be wrap-safe (32-bit `TickCount`). Put it in a pure helper with host unit tests. Known remaining risk: a server
   may close an idle connection during a long hold, which is an ordinary fetch failure.
3. **Deliver and check the production window app.** At the time of writing, the copy of `Weather` on maul is the older console
   build (about 1.86 MB), and the local `build/os9/Weather.pef` was about 274 KB, which looks too small for a window app that
   links TLS. Rebuild, explain the size, and deliver the production window app before judging it on the Mac.

## Smaller review findings on the window (not decided; fix or answer in the PR)

* The seed-error message is keyed on a process-wide sticky error, so a later DNS or network failure is shown as a seed problem.
  Key it on the failing status instead.
* The About box cannot be dismissed while a fetch runs, because `WindowActions::apply` returns early for every action except Quit.
* The console app now aborts on a malformed `weather.conf` line; it used to warn and continue. Document or restore.
* The first click on an inactive window also triggers Refresh if it lands on the button.
* Direct draws outside an update event are not validated (`ValidRect`), which causes extra update events.
* The hardware run is programmatic (actions are driven directly), so real menu/mouse tracking, suspend/resume and the
  allocation-failure fallback remain untested.

## Documentation to change together with the code

True today, false once the follow-up lands:

* `README.md`, OS 9 section: the sentence that menus and dragging wait until a fetch finishes.
* `docs/pr-plans/os9-window.md` (if still present): "suppresses ... modal menu/drag tracking" during a fetch.
* `os9/weather.r`: the `cannotBackground` flag and the comment beside it.
* `docs/ARCHITECTURE.md`, "Mac OS 9 target", and `docs/STATUS.md`: the paused-ticks clock, background behavior, and the
  idle-connection risk above.
* `tools/deliver-os9.sh` and the OS 9 workflow in `AGENTS.md`: name `Weather` (window), `WeatherConsole` and `WeatherUITest`,
  and say how each is launched.
* `docs/STATUS.md` table: add the window row and the production-app delivery check once #11 is merged.

Not stale: `docs/OS9-SEEDS.md` already has the Recovery section that the About box points to.

## Acceptance for the follow-up

* Host: unit test for the paused-ticks arithmetic including `TickCount` wraparound; `make check` and `make check-san` green.
* Hardware: hold a menu open longer than the fetch timeout during a fetch and confirm the fetch still completes; confirm
  File > Quit works mid-fetch; click away to another application mid-fetch and confirm the fetch completes; confirm suspend and
  resume redraw correctly. State plainly which of these were observed and which were driven programmatically.
