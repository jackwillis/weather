# Platform support and release policy

Status: **proposed** (2026-10-25). This document defines *weather's* own support tiers; these are not NetBSD's official architecture support tiers. This is a planning policy, not a statement that all builds or releases already exist.

## What counts as a platform?

A platform is a particular **operating system + CPU architecture + ABI/runtime environment**. For example, NetBSD 11/macppc and NetBSD 11/amd64 are different release targets; Linux/x86_64 distributions can share a build target without necessarily sharing a universally compatible binary. The target name, toolchain, minimum OS baseline and actual execution environment must be recorded per release.

Release support requires more than a successful cross-compile. Track these states separately:
1. **Experimental build:** builds, but execution/package validation is incomplete.
2. **Tested:** runs relevant tests on the target OS/ABI or suitable emulation/VM.
3. **Released:** packaged, installable, exercised, documented and supported according to its tier.

Don't label an artifact supported until its gate has passed.

## Five tiers

| Tier | Platforms | Commitment |
| --- | --- | --- |
| **1 — Primary** | **NetBSD 11/macppc (32-bit big-endian PowerPC)**; **Mac OS 9.2.2/PowerPC** | Core product targets. Real Power Mac G4 acceptance, OS-native presentation, release-blocking regressions. |
| **2 — Initial supporting** | **NetBSD 11/amd64**; **Linux/x86_64** (record libc/baseline distribution) | Build, test, package and run in native/VM environments; regressions block that target's release. |
| **3 — Next expansion** | Linux/aarch64; NetBSD/evbarm-aarch64; modern macOS/arm64 | Build/port experiments first; promote to releases only once executable tests and packaging exist. |
| **4 — Legacy/interesting** | Mac OS X 10.4/PowerPC; NetBSD/sparc64; Linux/ppc64le | Research and compatibility ports; non-blocking for primary releases. Native Tiger stays parked pending explicit approval. |
| **5 — Experimental** | NetBSD/mac68k; NetBSD/sparc; Windows/x86_64; other historic systems | No scheduled release or support promise. Accept experiments subject to testing and maintenance capacity. |

Tier numbers reflect *weather* priorities rather than platform quality or the upstream OS's own classification. A target may remain an experiment indefinitely.

## Initial release matrix: four targets

| Target | Programs | Distribution | Acceptance gate |
| --- | --- | --- | --- |
| NetBSD 11/macppc | `weather`, `xweather` | Native `pkg_add`-installable `.tgz`, with pkgsrc source package as the longer-term maintenance path | Cross-build on Linux; actual PowerPC tests and smoke checks on maul; installation/uninstallation check; owner verifies real X display |
| Mac OS 9.2.2/PowerPC | Native `Weather` app (plus diagnostic app if useful) | Classic Mac disk image (`.dsk` initially); preserve forks/resources | Retro68 build; launch, UI, TLS and cancellation tests on the Power Mac via Classic and/or native OS 9, with execution environment explicitly recorded |
| NetBSD 11/amd64 | `weather`, `xweather` | Native `pkg_add` `.tgz`, preferably generated from the same pkgsrc metadata | Build and exercise package in a NetBSD/amd64 VM, including CLI and X11 smoke |
| Linux/x86_64 | `weather`, `xweather` where X11 dependencies are available | Versioned `.tar.gz` with checksums, install and usage instructions | Native host/CI tests, sanitized/GUI checks, clean unpack and smoke; record distribution, libc and linking assumptions |

**Initial release does not mean simultaneous readiness.** Ship a target when its own gate is met; keep a release manifest of which targets are actually available. Do not let Tier 2 packaging delay a functional Tier 1 release unnecessarily.

For Linux, **do not add .deb/.rpm/Flatpak yet**; those are separate packaging projects after the portable tarball is proven. For NetBSD, avoid indefinitely hand-maintaining architecture-specific package definitions: aim for a shared pkgsrc source package and arch-specific binary output. Mac OS 9 seed files are per-installation secrets and must **never** ship inside a public disk image or release tarball; document safe provisioning separately.

## Sequencing

1. Stabilize the NetBSD/macppc `pkg_add` package and test install/uninstall on real hardware; complete the outstanding real-display review. This is the highest packaging priority.
2. Finish the Mac OS 9 window and validate a distributable resource-correct image; preserve secure per-installation seed provisioning.
3. Reproduce NetBSD/amd64 builds and installation in a VM, preferably sharing pkgsrc metadata with macppc.
4. Publish a Linux/x86_64 tarball built from a clean checkout and verified on a declared baseline environment.
5. Only after the four-target baseline is practical, investigate Tier 3. Prioritize Linux/aarch64 and NetBSD/aarch64 for ARM verification; modern macOS is optional and need not block the main project.

## Promotion criteria and maintenance

A new target needs an owner or maintainer, an explicit toolchain, a minimum tested OS baseline, actual execution tests (not just compilation), package contents/install/uninstall verification, and a documented way to reproduce release artifacts. Prefer at least one target-side CI or repeatable manual smoke procedure. Keep evidence of real hardware versus VM/emulation separate.

Promotion must offer a real benefit: a new OS interface/ABI/endianness challenge, an audience, or a deliberate packaging skill. A green compiler alone is insufficient. Demote or discontinue targets when there is no viable test environment or maintenance capacity.

For every release, record target triple, OS version, compiler/toolchain, library linkage, checksums, artifact name, program availability, tests executed, known limitations and whether real hardware was used. Keep portable core and platform adapters separate; don't widen the shared core's dependency requirements merely to serve a lower-priority platform.

## Deliberately out of scope for the first release

No Windows port, Linux Flatpak/.deb/.rpm, macOS Homebrew/DMG, Tiger-native build, SPARC package, ARM64 release commitment, public reusable-library extraction, or daemon. These remain possible follow-up work, not initial-release dependencies.

Relevant existing work: release readiness PR #18, native Tiger feasibility PR #21, NetBSD binary packaging scripts, OS 9 window and preferences work. Review this policy alongside those plans before assigning implementation.
