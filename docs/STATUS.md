# Status (2026-10-26)

## What works, and how it was checked
| Piece | State | Evidence |
|---|---|---|
| `weather` CLI, NetBSD/macppc | works | live fetch on maul; `make check-maul` 226 unit + 74,576 mutated inputs, `make smoke-maul` 10 checks (2026-10-25) |
| `xweather`, NetBSD/macppc | works in Xvfb on the laptop and on maul | 21-step GUI scenario; ~3 MB RSS, ~0 idle CPU. Not yet judged by a person on maul's real display |
| Data sources | `open-meteo`, `met-no` (needs `contact`) | both live-verified through BearSSL; fallback and skip-without-contact tested |
| Host test suite (`make check`) | green | lint, anchors, 461 unit, 186,018 mutated inputs, 78 CLI feature, 21 GUI checks, 2 provisioning tests; `make check-san` (UBSan+ASan) green |
| NetBSD package | built, staged | `tools/mkpkg-netbsd.sh`; the old build is installed on maul, the rebuilt one (with met-no) is not yet |
| Mac OS 9, console app | builds; production TLS path verified with managed seed | real storage/failure probes pass; approved seed installation and two live certificate-verified TLS launches with seed advancement passed on maul (2026-10-25) |
| Mac OS 9, window UI | implemented on **unmerged PR #11**, not on `main` | branch reports native QuickDraw tests under Classic and live TLS; pending merge/revalidation |

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
* The GitHub repository exists at https://github.com/jackwillis/weather. Open-Meteo and MET Norway licensing, attribution and release terms still need a final review.
* Delivery of OS 9 apps depends on the Mac being in OS X with a console login; a stuck `hdiutil attach` must be killed with `kill -9`.

## Current integration and release priorities

The default branch is `ebeaaed` (project-management78 merged). Main Checks and Policy passed. Earlier fixes32/35/36/38/39 are merged; their older open-work references are historical. The native window11 and its feature stack remain unmerged, despite substantial branch verification.

See [the complete integration assessment](INTEGRATION-2026-10-26.md) for all open PRs, current owner decisions, review order and remaining limits. Real milestones and tracker issues45–48 are now available; some old tracker prose still needs cleanup.

Next: repair shared labeler95, review the small correctness/test batch, implement decided TLS/test-build isolation66, stale display49/62 and manual refresh floor92, then reconcile the native window/lifecycle stack for one hardware acceptance session when Jack authorizes the required boot. Research is tracked separately and does not automatically block v0.1.

The existing detailed test counts above are historical evidence, not fresh verification of a combined release candidate. Separate green PRs do not establish that their combined tree passes.

## Reproduce
    make check                  # host: lint, anchors, unit, robustness, feature, GUI
    make check-san              # sanitizers
    make check-maul smoke-maul  # PowerPC tests on maul (booted into NetBSD)
    make os9                    # Mac OS 9 app (needs podman); deliver with tools/deliver-os9.sh
    make dist distcheck         # reproducible tarball
