# Handoff notes for coding agents (Codex, Claude Code, ...)

Read `docs/STATUS.md` (what works), `TODO.md` (what is next), `docs/ARCHITECTURE.md` (layers, seams, decisions) first.
This file is the working agreement. Updated 2026-10-25.

## Rules set by the owner (do not break these)
* **Never compile on maul** (the Power Mac G4 test machine, 256 MB RAM). Cross-build on the Linux laptop; maul only runs tests.
* **Privileged steps are the owner's.** You have no root on maul. Write a small reviewed script, push it with `maulctl push`, and ask the owner to run it
  (`runlog <name>`); never try to work around permissions.
* **Do not push to GitHub or create the repo**; commit locally at green milestones only. Commit messages end with the agent's co-author line.
* **No Apple material in the repo.** Apple SDK files were only read for reference (outside the repo, `~/scratch/os9-sdk`); our Open Transport
  declarations and glue are our own code. Never copy Apple headers or binaries into the tree.
* **Never invent contact details** (MET Norway needs a real `contact` in the user's config; do not use the owner's email for external services).
* **Do not click or answer system dialogs on the owner's OS 9 volume**; ask. Do not read private user data beyond what a task needs.
* Report faithfully: what was verified, what was not, what is a guess. A failing test is reported with its output.

## Engineering rules of this project
* C++17, `-fno-exceptions -fno-rtti`, no heap in `src/core` and `src/app`, errors as values (`Status`), no libstdc++ in NetBSD binaries (`nocxx.cpp`).
* Dependencies point down only: core/app (freestanding) < tls < platform/* < cli/ui. `make lint` enforces it for core and app.
* Red/green: write the failing test first. Keep `make check` green; also `make check-san`. Any new parser input path gets mutation coverage (`test/robust.cpp`).
* Retro68 quirks: no `%z`/`%ll` in printf, no Open Transport/`Microseconds`/`ReadLocation`/`GetKeys` headers (we declare what we use), an event pump must call
  `BeginUpdate/EndUpdate` for update events and bound its work per call.

## Build and test
    make check            # host: lint, anchors, unit (226), robustness (164k inputs), CLI feature (78), GUI in Xvfb (21)
    make check-san        # sanitizers
    make check-maul smoke-maul   # PowerPC tests on maul; maul must be booted into NetBSD (maulctl status)
    make os9              # Mac OS 9 app via Retro68 in podman -> build/os9/Weather.{APPL,bin,dsk}
    make package-netbsd   # build/pkg/weather-0.1.0.tgz (pkg_create runs on maul); install is `doas pkg_add` by the owner
    make dist distcheck   # reproducible tarball (needs a clean, committed tree)
The GUI test uses Xvfb display :88: never run two GUI runs at once (a collision once produced a false 10-check failure).
Toolchains: NetBSD cross compiler in `~/scratch/netbsd-xc` (see its `README-repro.md`); Retro68 container `ghcr.io/autc04/retro68`.

## Reaching the test machine (details live outside the repo)
`~/.local/bin/maulctl` is the harness: `maulctl status`, `maulctl nb '<cmd>'` (maul NetBSD), `maulctl osx '<cmd>'` (maul Mac OS X, which hosts
Classic / OS 9), `maulctl push nb|osx FILE NAME` (lands in /tmp, wiped at reboot). Access notes: `~/scratch/maul-lubulgi-writeup.md` and the
agent memory directory. maul boots either NetBSD or Mac OS X; ask the owner to reboot into the one you need.

## Mac OS 9 workflow (hardware-verified 2026-10-25)
1. `make os9`, then `maulctl push osx build/os9/Weather.dsk Weather.dsk` and `maulctl push osx tools/deliver-os9.sh deliver-os9.sh`.
2. Kill a running Classic if an old copy is loaded: `maulctl osx 'for p in $(ps ax | grep "[C]lassic Startup" | awk "{print \$1}"); do kill -9 $p; done'`.
3. `maulctl osx 'sh /tmp/deliver-os9.sh /tmp/Weather.dsk Weather'` in the FOREGROUND (backgrounding makes `hdiutil attach` fail; a stuck one needs `kill -9`).
4. `maulctl osx 'open /Volumes/MacOS9Lives/weather-dev/Weather'`. This needs the owner logged in as `claude` at the Mac console. Classic cold start takes ~2 min and the
   first `open` is often dropped: poll `weather.log`, re-open once.
5. Read results: `maulctl osx 'cat /Volumes/MacOS9Lives/weather-dev/weather.log'`; screenshot: `/usr/sbin/screencapture -x /tmp/s.png`, then
   `openssl base64 < /tmp/s.png` and decode on the laptop (Tiger has no `base64`). You cannot click or type on the Mac.
Check clocks with `date -u +%s` on the Mac versus the app's `clock:` log line.

## State at handoff
* Done and verified: CLI and xweather on NetBSD/macppc, two data sources, TLS via BearSSL, packaging, tests, OS 9 console app fetching live over TLS.
* In flight: the weather art was moved to `src/app/weather_art.hpp` (shared canvas interface; xweather adapted and verified visually and by `make check-gui`).
  The OS 9 window UI will use the same header with a QuickDraw canvas.

## Next tasks, in order
1. **OS 9 window UI** in `src/ui/os9/` on top of `app/Model`: window (NewCWindow), menus (Apple/File/Options), picture via `art::picture` with a QuickDraw canvas,
   temperature/wind/humidity text, status line from `Model::status_line`, event loop with `WaitNextEvent` and the fetch run through `Net::yield`.
   Then a Location dialog (reuse `app/LocationForm` + `TextEdit`), settings file in the Preferences folder, a `SIZE` resource of 3 MB.
   Test by self-driving: a scripted event mode plus `screencapture`, since clicks are impossible.
2. Extract pure helpers from the OS 9 layer (UTC conversion, printf format rewrite) so they get host unit tests.
3. OS 9 DNS timeout/cancel (async open with a notifier); better entropy if an OS source exists.
4. On-disk stale cache; third source (api.weather.gov needs multi-step requests); geocoding.
5. The owner must reinstall the rebuilt NetBSD package (`doas pkg_add -u`) and look at xweather on maul's real display.
