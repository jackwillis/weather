# Handoff notes for coding agents (Codex, Claude Code, ...)

Read `docs/STATUS.md` (what works), `TODO.md` (what is next), `docs/ARCHITECTURE.md` (layers, seams, decisions) first.
This file is the working agreement. Updated 2026-10-25.

## Rules set by the owner (do not break these)
* **Never compile on maul** (the Power Mac G4 test machine, 256 MB RAM). Cross-build on the Linux laptop; maul only runs tests.
* **Privileged steps are the owner's.** You have no root on maul. Write a small reviewed script, push it with `maulctl push`, and ask the owner to run it
  (`runlog <name>`); never try to work around permissions.
* **GitHub work uses feature branches and PRs in the existing repo**, as authorized by Jack on 2026-10-25. Do not create another repo.
  Commit at green milestones; commit messages end with the agent's co-author line. Merge only when Jack authorizes it.
  **Standing authorization (Jack, 2026-10-26, "you can merge non code"):** an agent may merge a PR whose changes are only Markdown or plain-text documentation (README, TODO, CHANGELOG, `docs/`) once hosted checks are green, with a merge commit, and unless it is labelled `needs-owner` or `blocked`. Anything touching code, tests, build files, scripts, workflows, man pages or notices still needs Jack. Tell Jack what was merged.
* **No Apple material in the repo.** Apple SDK files were only read for reference (outside the repo, `~/scratch/os9-sdk`); our Open Transport
  declarations and glue are our own code. Never copy Apple headers or binaries into the tree.
* **Never invent contact details** (MET Norway needs a real `contact` in the user's config; do not use the owner's email for external services).
* **Do not click or answer system dialogs on the owner's OS 9 volume**; ask. Do not read private user data beyond what a task needs.
* Report faithfully: what was verified, what was not, what is a guess. A failing test is reported with its output.

## Engineering rules of this project
* C++17, `-fno-exceptions -fno-rtti`, no heap in `src/core` and `src/app`, errors as values (`Status`), no libstdc++ in NetBSD binaries (`nocxx.cpp`).
* Dependencies point down only: core/app (freestanding) < tls < platform/* < cli/ui. `make lint` enforces it for core and app.
* Red/green: write the failing test first. Keep `make check` green; also `make check-san`. Any new parser input path gets mutation coverage (`test/robust.cpp`).
* Shell in `tools/` and `test/` is POSIX `sh`/`awk` (NetBSD's awk is BSD awk; no gawk idioms). The Makefile must keep working with GNU make 3.81 (macOS):
  no `$(file)`, no features newer than 3.81. `make check` is expected to pass on Linux and macOS (README, "Host platforms").
* Retro68 quirks: no `%z`/`%ll` in printf, no Open Transport/`Microseconds`/`ReadLocation`/`GetKeys` headers (we declare what we use), an event pump must call
  `BeginUpdate/EndUpdate` for update events and bound its work per call.

## Build and test
    make check            # host: lint, anchors (+14 tests of the check), unit (461), robustness (186k inputs), CLI feature (78), seed provisioning (2),
                          # Makefile flags (6), GUI in Xvfb (21; where X11 headers exist)
    make check-san        # sanitizers
    make check-maul smoke-maul   # PowerPC tests on maul; maul must be booted into NetBSD (maulctl status)
    make os9              # Mac OS 9 app via Retro68 in podman -> build/os9/Weather.{APPL,bin,dsk}
    make package-netbsd   # build/pkg/weather-0.1.0.tgz (pkg_create runs on maul); install is `doas pkg_add` by the owner
    make dist distcheck   # reproducible tarball (needs a clean, committed tree)
Host GUI checks allocate a private Xvfb display. Run all GUI checks serially, including checks with private displays, as directed by the owner.
Concurrent non-GUI checks require separate source/build/output trees; never compile into the same object directories.
The maul GUI check uses display :77. Older branches with fixed :88/fixture ports also require coordination.
Toolchains: NetBSD cross compiler in `~/scratch/netbsd-xc` (see its `README-repro.md`); Retro68 container `ghcr.io/autc04/retro68`.

## Reaching the test machine (details live outside the repo)
`~/.local/bin/maulctl` is the harness: `maulctl status`, `maulctl nb '<cmd>'` (maul NetBSD), `maulctl osx '<cmd>'` (maul Mac OS X, which hosts
Classic / OS 9), `maulctl push nb|osx FILE NAME` (lands in /tmp, wiped at reboot). Access notes: `~/scratch/maul-lubulgi-writeup.md` and the
agent memory directory. maul boots either NetBSD or Mac OS X; ask the owner to reboot into the one you need.

## Mac OS 9 workflow (hardware-verified 2026-10-25)
1. `make os9`, then `maulctl push osx build/os9/Weather.dsk Weather.dsk` and `maulctl push osx tools/deliver-os9.sh deliver-os9.sh`.
2. Quit an old app normally before replacing it. Do not force-kill Classic for routine updates: it can leave a TLS seed lock. If a crash left a lock, follow `docs/OS9-SEEDS.md` recovery.
3. `maulctl osx 'sh /tmp/deliver-os9.sh /tmp/Weather.dsk Weather'` in the FOREGROUND (backgrounding makes `hdiutil attach` fail; a stuck one needs `kill -9`).
4. `maulctl osx 'open /Volumes/MacOS9Lives/weather-dev/Weather'`. This needs the owner logged in as `claude` at the Mac console. Classic cold start takes ~2 min and the
   first `open` is often dropped: poll `weather.log`, re-open once.
5. `Weather` is now the window app; use `WeatherConsole.dsk`/`WeatherConsole` for console diagnostics. Build optional `WeatherUITest` with `WEATHER_BUILD_DIAGNOSTICS=ON`; `python3 tools/check-os9-window.py /tmp/weather-window-shots` drives it and saves screenshots. Never run two test drivers at once.
6. Read results: `maulctl osx 'cat /Volumes/MacOS9Lives/weather-dev/weather.log'`; screenshot: `/usr/sbin/screencapture -x /tmp/s.png`, then
   `openssl base64 < /tmp/s.png` and decode on the laptop (Tiger has no `base64`). You cannot click or type on the Mac.
Check clocks with `date -u +%s` on the Mac versus the app's `clock:` log line.

## State at handoff
* CLI and xweather work on NetBSD/macppc; two providers, BearSSL TLS, packaging and host checks are in place.
* OS 9 Weather is the native window; WeatherConsole retains the console diagnostic. They share bounded, all-or-nothing Preferences/legacy weather.conf loading (see docs/OS9-PREFERENCES.md).
* WeatherUITest is a separate optional executable. Production Weather links no fixture generator or marker-file driver. See README for the scripted hardware run.
* DNS/managed TLS seeds are implemented; preserve once-per-process network init and terminal shutdown. Never remove a live seed lock.

## Next tasks
**Finishing releases without the owner present:** follow the runbooks. v0.0.1 (a rough pipeline pre-release, milestone v0.0.1): issue #123. v0.1: the "How to finish v0.1" section of issue #45. Both list what only Jack can do (merge authorization, reboots, privileged installs, tag and publish); label and move on when blocked.

1. Finish #12 (Location dialog and persistent Preferences, #103): native UI scenario and owner typing/relaunch persistence have passed; merge needs Jack's authorization. Remaining: read-only volume and power-loss limits stay documented, not verified.
2. Complete stalled DNS/delayed-open and NetBSD hardware acceptance (#14); owner controls reboots and privileged installation.
3. Last-good forecast cache (#15), NWS (#16, only api.weather.gov), location search (#17).
4. Release/terms/package acceptance (issue #67) and big-endian CI (#19, follows #10).
5. Config reload is conditional (issue #68). Tiger remains parked (issue #69).
6. **UX** (forecast content, pinned locations, charts, native look): read issue #113 first, then your platform's issue (#98 Mac OS 9, #99 NetBSD/X11). The first data slices are #117 (hourly/daily data) and #118 (pinned locations), both v0.2. Map research is #114. Amend those issues; do not open parallel ones.

## Project management
Issues, milestones, labels, the board and the PR flow are described in `docs/PROJECT-MANAGEMENT.md`. Start work from an issue, link it from the PR (`Closes #N`),
set `agent/codex` or `agent/claude` while you own it, and use `needs-owner` / `needs-hardware-*` / `blocked` instead of leaving a PR silently stuck.

