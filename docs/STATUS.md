# Status (2026-10-28)

## Current release and integration state

The live release gate is [#45](https://github.com/jackwillis/weather/issues/45).
PRs #11/#12 (window/Preferences), #15 (cache), #41 (deadlines), #145 (TLS policy),
#146 (MET revalidation), #147 (diagnostic entropy probe) and #163 (automatic
fallback) have merged. Their implementation and remaining acceptance are separate.
The latest hosted main Checks run at e0dc35e passed on 2026-10-28; this is not a
new local or hardware test run. Test counts below belong to the dated evidence.

Initial Classic and native OS 9 timing captures exist (#121); source assessment,
restart/repeated sampling, credited entropy and a justified threshold do not.
The analyzer still credits zero bits and production still requires provisioning.
On October 27 Jack observed production Weather across a native OS 9 restart;
this is owner evidence, not a complete native acceptance suite. Classic window
and cache diagnostics also ran; #45/#102/#128 record their scope. Command-period
cancellation (#168) and system-requested quit/Tiger logout (#169) are known open
window defects. Native MET/cache-saving and real-display NetBSD acceptance remain.

As of this audit, PR #154 is non-draft and mergeable; #14 is draft and mergeable.
Draft PRs #16/#17/#19/#85/#87/#91 conflict with current main despite green recorded
checks. Refresh and combined checks are required before review readiness.

Roadmap: v0.2 covers resilience, sources/search and native Preferences (#182);
hourly/daily data (#117), pins (#118) and window states (#176) are v0.3; CLI/docs
revision (#178/#179) is v0.4. #181 records platform configuration idioms and the
pre-1.0 no-migration policy. Existing text Preferences remain implemented until
#182 lands. GitHub milestones supersede historical tier/roadmap descriptions.

## Automatic source fallback (#152)
New installations try Open-Meteo then MET Norway, identifying MET requests with the public project repository URL unless `contact` overrides it. MET responses carry server expiry and are reused before it; a retained MET record survives a later Open-Meteo success. Saving off removes both records and skips MET. Host check/check-san passed (1823 unit checks in each mode, 426598 mutated inputs, 119 CLI checks in each mode, 55 GUI checks); OS 9 diagnostics and NetBSD CLI/X11/unit/robust cross-builds passed. Native execution on NetBSD/macppc or Classic/native OS 9 is not verified for this change. See docs/verification/met-default-fallback-2026-10-27.txt. This does not complete the owner’s provider-terms or hardware acceptance gates.

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
| Data sources | `open-meteo`, `met-no` (project identity by default) | historical live BearSSL checks; current identity/fallback host-tested, native acceptance pending |
| Host test suite (`make check`) | hosted main Checks green at e0dc35e (2026-10-28) | latest detailed local counts are in the automatic-fallback evidence above; no fresh local suite run for this documentation audit |
| NetBSD package | v0.0.1 install/run/remove verified; current candidate reinstall pending | see release evidence above; do not assume old `/tmp` staging survived a reboot |
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

## Historical window implementation evidence (merged PR #11)

Menu/drag/close tracking now credits a shared active-time clock used by DNS and
socket/TLS deadlines. Background execution and explicit suspend/resume handling
are implemented; local deadline pauses cannot prevent a server closing an idle
connection. Host checks and cross-builds pass. New background/menu acceptance and
production delivery await a Tiger boot; earlier native UI evidence remains valid
for its stated scope. The 274 KB native PEF includes TLS, whereas the larger
console target additionally links RetroConsole. Production delivery and Classic/native observations advanced on October 27; see the current summary and #102 for the exact coverage.

NetBSD follow-up regression (2026-10-25, #11 combined with main): actual PowerPC
run passed 535 unit checks and 80820 robustness inputs. All ten smoke checks
passed, including live TLS, clock/anchor failures and settings. Host check and
ASan/UBSan passed as well. No builds ran on maul.

## Location/Preferences (merged PR #12)

Native dialog, save-before-apply settings transactions and checked File Manager
Preferences storage are implemented and cross-built. Host policy/mutation tests
and native NetBSD portable-policy runs pass; exact final counts are in the PR
plan. Native dialog/store scenarios and owner typing/relaunch acceptance subsequently
passed as recorded in #103 and the handoff. Locked/read-only volume and physical
power-loss limits remain unverified; do not infer these from restart success.
See OS9-PREFERENCES.md. Replacement with native typed Preferences is #182,
not an unmerged part of #12.

## Fetch deadlines (merged PR #41) (2026-10-27)

Main includes the deadline and OS 9 window/Preferences changes.
Net exposes monotonic milliseconds separately from UTC certificate time; OS 9
uses the shared modal-credit active clock. Host tests cover paused menus, UTC
steps, missing/regressing clocks and attempt/caller budgets. Exact stream expiry
and a clock regression above the attempt start have explicit red/green tests.
Current validation and limits are recorded on PR #41. Blocking POSIX getaddrinfo
and native OS 9 TCP/stream budget enforcement remain outstanding. No new native
OS 9 runtime acceptance is claimed.

## OS 9 entropy research (#120 / #121, 2026-10-27)

A diagnostic-only EntropyProbe and a bounded offline analyzer are prepared.
They collect timings from owned resources without reading seeds or logging key
contents. Initial Classic/native captures are recorded in #121. The analyzer always
credits zero bits; no source credit, production collector or threshold is claimed. Production provisioning
is unchanged. See OS9-ENTROPY-MEASUREMENT.md for the owner runbook and review gate.

## Forecast cache integration (merged PR #15) (2026-10-27)

The merged cache preserves the five-second manual
refresh floor, live-data unknown-age handling and persistent OS 9 Preferences.
Host unit/storage and mutation checks pass; full combined-suite verification is
recorded in the PR. Earlier NetBSD hardware results apply to the earlier heads
only. OS 9 cache storage-fault and window runtime acceptance remain pending;
see docs/FORECAST-CACHE.md.

## MET conditional requests in #128 (2026-10-27)

Merged PR #146 builds on cache PR #15: bounded Last-Modified persistence, matching
If-Modified-Since requests and safe 304 reuse are implemented through the shared
CLI/X11/OS 9 path. Cache record v2 is 224 bytes; unsupported draft v1 records are
refetched. Host/GUI and cross-build results are recorded in the implementation
PR. Forecast saving defaults on, with a persisted Options setting on both window
apps. Turning it off deletes the saved forecast and disables MET Norway with an
explanation; Open-Meteo remains available without disk storage. Native runtime
acceptance remains pending.

## TLS policy in issue #66 (2026-10-27)

Merged PR #145 restricts negotiation to TLS 1.2 and six ECDHE AEAD suites.
All four provider/geocoding services passed certificate-verified handshakes with
Weather's production client. Production omits the environment clock/trust hooks;
explicit test builds retain them in separate output paths. Host fixture tests
reject CBC-only and TLS-1.1-only peers. Native execution is not claimed; see
TLS-POLICY.md and verification/tls-policy-2026-10-27.txt.
