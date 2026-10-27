# Status (2026-10-27)

## Releases
**0.0.1 (pre-release), published 2026-10-27:** https://github.com/jackwillis/weather/releases/tag/v0.0.1 (tag at 59b50f0). Built by
the tag workflow (source tarball and Mac OS 9 `.dsk` on a GitHub runner) with the NetBSD package built on the laptop and maul and
attached by `tools/release-attach.sh`. Checked: tarball byte-identical to a local `make dist`; package installed, run and removed
on maul (`runlog pkgtest-weather`); the `.dsk` Weather launched in Classic and fetched live over TLS; the OS 9 About box viewed
on the real screen (page 1 and 3 of 5). Licence GPL-3.0-or-later. Known issues are in `docs/release-notes/0.0.1.md`.
The tree is back at the unreleased 0.1.0. Still to do for v0.1: #45.


## What works, and how it was checked
| Piece | State | Evidence |
|---|---|---|
| `weather` CLI, NetBSD/macppc | works | live fetch on maul; `make check-maul` 226 unit + 74,576 mutated inputs, `make smoke-maul` 10 checks (2026-10-25) |
| `xweather`, NetBSD/macppc | works in Xvfb on the laptop and on maul | 21-step GUI scenario; ~3 MB RSS, ~0 idle CPU. Not yet judged by a person on maul's real display |
| Data sources | `open-meteo`, `met-no` (needs `contact`) | both live-verified through BearSSL; fallback and skip-without-contact tested |
| Host test suite (`make check`) | green | lint, anchors, 461 unit, 186,018 mutated inputs, 78 CLI feature, 21 GUI checks, 2 provisioning tests; `make check-san` (UBSan+ASan) green |
| NetBSD package | built, staged | `tools/mkpkg-netbsd.sh`; the old build is installed on maul, the rebuilt one (with met-no) is not yet |
| Mac OS 9, console app | builds; production TLS path verified with managed seed | real storage/failure probes pass; approved seed installation and two live certificate-verified TLS launches with seed advancement passed on maul (2026-10-25) |
| Mac OS 9, window UI | native window implemented; follow-up hardware checks pending | separate scripted hardware scenario exercises artwork, units, stale errors, update events during fetch, live TLS and quit; shared config read checks; see PR #11 |

## Mac OS 9 findings (all on real hardware)
* Classic runs on the Power Mac G4 under Mac OS X Tiger; apps are delivered and launched from Linux over SSH (`tools/deliver-os9.sh`,
  needs a console login of the `claude` user for `open`).
* Open Transport works from Retro68 apps. Retro68 has import libraries but no headers; Apple's static glue is not needed: our
  `ot_glue.cc` reproduces it (a 40-byte client record passed as the last argument of each `*Priv` call).
* The Mac clock is local wall time; `gmtDelta` already includes daylight saving (verified against OS X).
* Retro68 `printf` lacks `%z` and `%ll`; the OS 9 log rewrites `%z`, and 64-bit values are printed as `unsigned long`.
* An event pump must validate update events (`BeginUpdate`/`EndUpdate`) and bound its work per call, or it spins forever.

## Known gaps and risks
* OS 9 now requires a provisioned managed seed, with checked rotation and exclusive ownership. Host tests, real-adapter fault injection, approved installation and two live TLS launches on maul pass. Import cleanup uses native File Manager deletion because Retro68's unlink is a stub; regression-tested on hardware. Supplemental jitter has no measured entropy guarantee, and physical power-loss/rollback durability is unverified; see OS9-SEEDS.md.
* OS 9 DNS now uses an async provider with yield/cancel and a wrap-safe deadline. Host lifecycle tests pass; DnsProbe and DnsNetProbe passed on maul (2026-10-25), including closing a pending query and reconnecting. A deliberately stalled DNS server and delayed-open cancellation remain unverified on hardware. POSIX `getaddrinfo` still blocks.
* Feature branches and PRs are on GitHub. Open-Meteo and MET Norway terms still need a final read before release.
* Delivery of OS 9 apps depends on the Mac being in OS X with a console login; a stuck `hdiutil attach` must be killed with `kill -9`.

## Reproduce
    make check                  # host: lint, anchors, unit, robustness, feature, GUI
    make check-san              # sanitizers
    make check-maul smoke-maul  # PowerPC tests on maul (booted into NetBSD)
    make os9                    # Mac OS 9 app (needs podman); deliver with tools/deliver-os9.sh
    make dist distcheck         # reproducible tarball

## Window follow-up in PR #11

Menu/drag/close tracking now credits a shared active-time clock used by DNS and
socket/TLS deadlines. Background execution and explicit suspend/resume handling
are implemented; local deadline pauses cannot prevent a server closing an idle
connection. Host checks and cross-builds pass. New background/menu acceptance and
production delivery await a Tiger boot; earlier native UI evidence remains valid
for its stated scope. The 274 KB native PEF includes TLS, whereas the larger
console target additionally links RetroConsole. The old production Weather on
maul has not yet been replaced.

NetBSD follow-up regression (2026-10-25, #11 combined with main): actual PowerPC
run passed 535 unit checks and 80820 robustness inputs. All ten smoke checks
passed, including live TLS, clock/anchor failures and settings. Host check and
ASan/UBSan passed as well. No builds ran on maul.

## Location/Preferences in PR #12

Native dialog, save-before-apply settings transactions and checked File Manager
Preferences storage are implemented and cross-built. Host policy/mutation tests
and native NetBSD portable-policy runs pass; exact final counts are in the PR
plan. The isolated PreferencesProbe and WeatherPrefsUITest are prepared but have
not run. Layout, typing, locked/read-only volume behavior, actual process restart
and production delivery await Tiger. See OS9-PREFERENCES.md for source precedence
and explicit failure/durability limits.

## Fetch deadlines in PR #41 (2026-10-27)

The deadline branch now incorporates main's OS 9 window/Preferences changes.
Net exposes monotonic milliseconds separately from UTC certificate time; OS 9
uses the shared modal-credit active clock. Host tests cover paused menus, UTC
steps, missing/regressing clocks and attempt/caller budgets. Exact stream expiry
and a clock regression above the attempt start have explicit red/green tests.
Current validation and limits are recorded on PR #41. Blocking POSIX getaddrinfo
and native OS 9 TCP/stream budget enforcement remain outstanding. No new native
OS 9 runtime acceptance is claimed.
