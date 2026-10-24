# Plan

Last updated: 2026-10-23. Keep this to a page or two; details go in the decision log and TODO.md.

## Goal
A small, well-behaved Unix weather tool for old big-endian machines, with a UI that looks like
its own era. Order of targets: NetBSD/macppc (maul), then Mac OS 9 (Retro68). Tiger-native is parked.
One portable core, one UI layer per OS. NetBSD gets a Motif-style window drawn in plain Xlib
behind a small Motif-shaped API ("Motif stub") so real Motif could replace it.

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
- Multiple data sources behind one provider interface, configurable fallback, on-disk cache that
  serves stale data (marked stale) instead of nothing.
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

## Open questions
- Which providers beyond Open-Meteo (MET Norway, NWS): TLS compatibility unchecked.
- Whether Retro68's GCC accepts this C++17 for OS 9; Open Transport usability.
