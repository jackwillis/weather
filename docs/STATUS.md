# Status (2026-10-25)

## What works, and how it was checked
| Piece | State | Evidence |
|---|---|---|
| `weather` CLI, NetBSD/macppc | works | live fetch on maul; `make check-maul` 226 unit + 74,576 mutated inputs, `make smoke-maul` 10 checks (2026-10-25) |
| `xweather`, NetBSD/macppc | works in Xvfb on the laptop and on maul | 21-step GUI scenario; ~3 MB RSS, ~0 idle CPU. Not yet judged by a person on maul's real display |
| Data sources | `open-meteo`, `met-no` (needs `contact`) | both live-verified through BearSSL; fallback and skip-without-contact tested |
| Host test suite (`make check`) | green | lint, anchors, 461 unit, 186,018 mutated inputs, 78 CLI feature, 21 GUI checks, 2 provisioning tests; `make check-san` (UBSan+ASan) green |
| NetBSD package | built, staged | `tools/mkpkg-netbsd.sh`; the old build is installed on maul, the rebuilt one (with met-no) is not yet |
| Mac OS 9, console app | builds; production TLS path verified with managed seed | real storage/failure probes pass; approved seed installation and two live certificate-verified TLS launches with seed advancement passed on maul (2026-10-25) |
| Mac OS 9, window UI | native window implemented | separate scripted hardware scenario exercises artwork, units, stale errors, update events during fetch, live TLS and quit; shared config read checks; see PR #11 |

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
