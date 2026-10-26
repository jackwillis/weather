# Plan

Current delivery priorities reviewed: 2026-10-25. Historical dated decisions below are retained as history, not always current implementation guidance.

## Goal
A small, well-behaved Unix weather tool for old big-endian machines, with a UI that looks like
its own era. Order of targets: NetBSD/macppc (maul), then Mac OS 9 (Retro68). Tiger-native is parked.
One portable core, one native UI per OS. NetBSD uses lightweight CDE-inspired Xlib drawing, **not** a Motif-compatible stub; classic Mac OS uses the Toolbox/QuickDraw work tracked in PR #11.

## Delivery roadmap (current)

**A — Weather release (blocking):** integrate reviewed audit/correctness fixes (#32, #35, #36, #38, #39, #41); merge/test OS 9 window #11 and location/settings #12; finish real-screen NetBSD acceptance, NetBSD package reinstall, release attribution and Macintosh distribution. Keep mainline C++17/exception-free and its network interfaces stable. Release without waiting for a shared SDK.

**B — independently maintained trust data (next product enhancement):** #40 proposes editable certificate source folders on a modern host, a deterministic installable trust snapshot, safe removal/distrust and native Mac/NetBSD loaders. A shared certificate store does not require CFM or a system extension. No implementation is on main.

**C — Mac-first networking research (non-blocking):** #27 and #42 explore static SDK, narrow C ABI and optional CFM redistributable; #43 audits the current reconstructed OT glue against public InContext APIs and OS-version support. CFM and OS 8/System 7 compatibility must be proven, not assumed. #37 studies C++20/23 feature compatibility; #19 considers big-endian CI. Tiger-native stays parked (#21).

**Release acceptance:** real hardware execution, TLS/clock/seed correctness, clear installation instructions, source attribution, tested packages, and truthful platform labels. Currently verified Mac evidence is **OS 9.2.2 in Classic under Tiger**, not native OS 9 boot or earlier System versions.

## GitHub milestone policy

Use **native GitHub Milestones** as the authoritative release planning mechanism. Create milestones named **v0.1**, **v0.2**, **v0.3 (provisional)**, and **Research / Incubation**; leave due dates unset until we have evidence-based schedules. Existing tracking issues [#45](https://github.com/jackwillis/weather/issues/45), [#46](https://github.com/jackwillis/weather/issues/46), [#47](https://github.com/jackwillis/weather/issues/47), and [#48](https://github.com/jackwillis/weather/issues/48) contain the initial scope and exit criteria. These issues are **temporary trackers**, not themselves native milestones.

Once the native milestones exist, **assign each actionable PR or issue to the appropriate milestone**, preserving uncommitted RFCs under Research / Incubation until explicitly promoted into a release. Use v0.1 for release-blocking implementation/hardware/packaging; v0.2 for cache and independent CA trust; v0.3 for conditional SDK work; and Research / Incubation for CFM feasibility, older systems, modern C++ and parked ideas. Do not make an experimental PR a release blocker merely by referencing it.

Before each release, review its milestone's open items and verify the acceptance checklist against **merged main** and real target hardware. Close or supersede the temporary tracking issues after their criteria have been transferred to milestone descriptions and actionable issues; avoid maintaining conflicting sources of truth. Maintenance bugs found after v0.1 may be tracked in a **v0.1.x** milestone when needed.

## Constraints
- Cross-compile only. maul has 256 MB RAM and is the test machine, never the build machine.
- 32-bit big-endian PowerPC. No Go. No heavy runtime: C++17 with -fno-exceptions -fno-rtti,
  header-only std (string_view etc.), no heap in the core where practical.
- Built-in network layer with TLS (BearSSL). No proxy.

## Quality bar ("old-school release engineering")
- Errors are values: every fallible call returns a status with a distinct kind (DNS, connect,
  TLS, certificate, timeout, HTTP status, parse, bad data). Bounded retries, timeouts everywhere.
- Logging: levels to stderr or syslog, -v/-q, request/response and TLS trace dumps for debugging.
- Unix behaviour: getopt, --version, usage, sysexits exit codes, config file with CLI overrides,
  signal handling, script-friendly one-line mode, man page.
- Multiple data sources behind one provider interface and configurable fallback. The Model retains last-good data **in memory**; an on-disk offline cache is proposed in #15 but is not implemented.
- Debuggability: symbols kept in a separate debug copy, offline fixtures, reproducible builds with
  recorded toolchain versions.
- Releases: BSD- and GNU-make compatible Makefile, check/dist/install (DESTDIR), versioned
  tarball with SHA-512 sums, changelog. Tests: static_assert at build time, native runtime tests,
  smoke test of the real PowerPC binary on maul.

## Milestones (each has an exit test)
1. TLS spike: BearSSL handshake and one JSON fetch from Open-Meteo, run on maul.
   Exit: JSON body printed on maul with the certificate chain verified against pinned anchors.
2. Core + CLI: provider interface, parsers, errors, logging, fixtures, `weather` prints a forecast.
   Exit: native tests pass; the PowerPC binary prints a live forecast on maul.
3. Motif-style Xlib window on the core. Exit: window shows live data on maul's X.
4. More providers, cache, man page, release tarball.
5. Later: Mac OS 9 (Retro68, Open Transport layer), tested first in Classic on Tiger, then on real OS 9; CI with a big-endian check.

## Working method
Small verified steps; commit at each green milestone with the exact commands recorded.
A second agent (e.g. Codex) is optional, used as an independent reviewer of risky code.

## Decision log
- 2026-10-23 C++17 header-only, plain C fallback (C++17 cross-compiles with this GCC 12.5;
  link with -Wl,--as-needed to avoid libstdc++).
- 2026-10-23 BearSSL for TLS (portable, no malloc, big-endian clean; builds for macppc unmodified).
  Risk: TLS 1.2 only. Fallbacks: mbedTLS or wolfSSL.
- 2026-10-23 Real Motif (OpenMotif/LessTif) not installed on maul; draw the look in Xlib instead.
- 2026-10-23 Host builds of NetBSD tools on Fedora need gcc -std=gnu17 (see netbsd-xc README).

- 2026-10-23 M1 done: tlsfetch (PowerPC, 117 KB stripped, needs only libc+libgcc_s) completes a TLS 1.2
  handshake (cipher 0xcca8, ChaCha20-Poly1305) and fetches JSON on maul. Anchors: ISRG Root X1 and X2,
  generated with brssl from Fedora's CA bundle (spike/ta.h). With no anchors it fails with err 62
  (not trusted), so verification is real. Server cert type seems to vary by client (openssl saw ECDSA,
  BearSSL got an RSA suite); X1 covers both.

- 2026-10-24 M2 done. Layering: Net/Io function pointers (the OS seam) -> fetch (retry, fallback, clock check)
  -> provider (path + parse). TLS is a wrapper Io, so the same fetch code runs plain (tests) or TLS.
  json_get validates the whole document before trusting any key (a red test found a truncated doc
  being half-accepted). libstdc++ avoided by defining std::__throw_out_of_range_fmt (src/nocxx.cpp).
  A plain-HTTP testing flag (-H) exists and warns loudly; WEATHER_NO_TA=1 is a test hook that must make TLS fail.
  Binary: 153 KB stripped, needs only libc + libgcc_s.

- 2026-10-24 Default location Madison, WI 53703 (Capitol Square 43.0747,-89.3841), with a `name` setting.
  Persistence: ~/.weatherrc is the single source of truth for settings shared by CLI and xweather; the
  window rewrites only the latitude/longitude/name lines (temp file + rename, other lines and comments kept).
  Appearance (fonts, colours, geometry) goes in X resources, optional. Each OS gets its own settings
  store behind a small interface (OS 9 Preferences folder); the core assumes no dotfile.

- 2026-10-24 License: BSD 2-clause (matches NetBSD/pkgsrc culture, pkgsrc name `2-clause-bsd`; compatible
  with BearSSL's MIT). Source hosted on GitHub (not created or pushed yet). Distribution goal: a personal
  `pkgin` repository (hand-built binary package on lubulgi), not pkgsrc proper or pkgsrc-wip for now.
- 2026-10-24 Packaging findings: pkgsrc cross-compilation works only from a NetBSD build host (not Fedora).
  No install script needed: pkgsrc installs from PLIST and our settings are a per-user ~/.weatherrc.
  Constraints for the build: `make install` with DESTDIR/PREFIX/MANDIR, honour CXX/CXXFLAGS/LDFLAGS, no
  hard-coded paths, `make dist` tarball + SHA-512, BearSSL switchable between external and vendored.
  maul's pkgin already uses the official repo https://cdn.NetBSD.org/pub/pkgsrc/packages/NetBSD/powerpc/11.0_2026Q2/All;
  pkg_install there is 20260227. Unverified: building pkg_install/pkg_create on Fedora, and how pkgin
  treats an unsigned second repository.

- 2026-10-24 xweather: CDE look (blue-grey, hand-drawn bevels in Xlib behind a small Motif-shaped API), auto-refresh
  every 15 min plus a Refresh button. Every OS gets its own UI. Full architecture, seams, target matrix, build
  system, test and release decisions: `docs/ARCHITECTURE.md`. Notable changes it makes: GNU make required;
  cooperative `yield` hook on the network seam so a UI never freezes during a fetch (no threads/fork);
  TLS gets time and entropy from the platform.

- 2026-10-24 Scope narrowed to NetBSD and Mac OS 9. OS 9 app is tested first in Classic on maul's Tiger side
  (UNVERIFIED that Classic works there), then on real OS 9. Tiger-native parked (needs a modern cross-GCC and the
  Apple SDK; options recorded in ARCHITECTURE.md).

- 2026-10-24 M3b done. Findings worth keeping: (1) `Net::now` was `long`, 32-bit on ppc32 and dead in 2038:
  epoch seconds are `long long` everywhere. (2) BearSSL seeds from the OS once, else needs injected entropy; we
  inject 32 bytes from the platform and refuse TLS without a source. (3) Certificate time comes from the platform
  clock (`br_x509_minimal_set_time`), proven by tests with WEATHER_NOW (year 2100 and early 2025 are rejected, 1970 gives
  the clock error). (4) `-c FILE` means the file must exist, except with `-S` which may create it. (5) A name containing
  '#' is rejected everywhere, since the file format could never read it back. (6) The unit tests now run on maul too.

- 2026-10-24 M3c done (pending a look on the real display). The "Motif stub" idea became plain CDE-look primitives (see
  ARCHITECTURE.md). Found by GUI tests: hover-switching menus made the next click close the menu (now only a click that
  opened a menu toggles it); Xlib's `Status` macro collides with wx::Status (`#undef` after the X headers); XSendEvent to
  window 0 means "the window under the pointer" (the test driver now refuses to send without a window).

- 2026-10-24 BearSSL is vendored at a pinned commit (third_party/README.md), not fetched: a fresh clone builds with no network. Anchors are generated from a PEM in the tree and checked.

- 2026-10-24 Packaging, first version: pkgsrc cannot cross-compile from Linux, so the binary package is assembled by
  `tools/mkpkg-netbsd.sh`: cross-build, stage with `make install` (PREFIX=/usr/pkg, MANDIR=/usr/pkg/man), then `pkg_create`
  runs on maul (only archives). BUILD_INFO mimics real packages (OPSYS/OS_VERSION/MACHINE_ARCH, REQUIRES for the base
  libs). pkg_install on maul is 20260227 (PKGTOOLS_VERSION field 20091115 as in real packages). Still to do: a pkgin
  repository on lubulgi, and a real pkgsrc source package.

- 2026-10-24 OS 9 gates: compile PASSED (Retro68 GCC 16.1.0 builds the portable core + compile-time tests for PowerPC and
  68k, C++17); networking OPEN (Retro68's headers lack Open Transport and MacTCP; see ARCHITECTURE.md for the options).
  Language level stays C++17 (policy in ARCHITECTURE.md). xweather measured on maul: ~3 MB resident, ~0 idle CPU.
  ctwm: weather added to the root menu and OccupyAll by tools/ctwm-weather.sh (marker blocks, backup, undo); optional
  autostart via tools/xinitrc-weather.sh. The pkgin repository on lubulgi is not needed: the .tgz is enough.

- 2026-10-24 Second data source: MET Norway (met-no). Chosen over api.weather.gov because it is one request and worldwide;
  NWS chains to a root we already trust but needs three requests. MET's terms require a User-Agent with contact details, so
  the provider is `needs_contact` and refuses (Err::Config, skipped in a fallback chain) until `contact` is set: no contact
  is ever invented. TLS check: leaf <- GEANT TLS RSA 1 <- HARICA TLS RSA Root CA 2021 (the server also sends a cross-signed copy
  of that root; BearSSL stops at the first certificate whose issuer is an anchor, so anchoring the self-signed root works,
  verified live). Root fingerprint D9:5D:0E:8E... matched my independent recollection and Fedora's bundle.
  macOS: jack logged into the Mac console as `claude`, so GUI apps can be launched over SSH and the screen read with `screencapture`.

## Current open questions
- When can native OS 9.2.2 and earlier versions be independently tested? Open Transport availability alone does not imply this binary works on Mac OS 8 or System 7.
- Can Retro68 emit and consume an actual shared CFM fragment? See #42 for the isolated spike; no claimed result.
- Can Apple-documented InContext APIs replace private OT glue reliably for a future shared library? See #43.
- NWS, geocoding and on-disk cache are future additions, not current provider capabilities.
