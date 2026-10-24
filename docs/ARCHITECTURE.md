# Architecture and engineering decisions

Written 2026-10-24, before the second target. Companion to `PLAN.md` (goals, milestones, dated log).
Items marked UNVERIFIED are beliefs to check before relying on them.

## 1. Layers and the dependency rule

    ui/<os>     xweather (CDE look, Xlib), os9            one UI per OS, deliberately different
    app/        Model: state machine + refresh timers      portable, unit tested
    core/       status text json http forecast provider    portable, constexpr, no OS headers
                openmeteo fetch config format
    tls/        BearSSL glue (needs time + entropy)         portable; gets both from the platform
    platform/<os>   net, clock, entropy, log sink, settings store     the only code that touches the OS

Rule: dependencies point down only. `core/` and `app/` include no OS headers (only freestanding
`<cstddef>`, `<string_view>`, ...). `make lint` greps `core/` and `app/` for OS includes and fails the
build on a hit, so the rule is enforced, not hoped for. No heap in `core/`; no exceptions, no RTTI.

## 2. The OS seams (function-pointer structs, as in `net.hpp`)
- **Net**: connect / close / sleep_ms / now, plus tls_open / tls_close.
- **Io**: read / write on a stream. Errors carry an `Err` kind, `errno`-style detail and a static message.
- **yield (new)**: `bool (*yield)(Net&)` called whenever the code would block (poll slices of ~50 ms,
  backoff sleeps). A UI uses it to pump its event loop (redraw "Fetching...", handle expose/close); returning
  false cancels the operation (`Err::Cancelled`, exit status 130, never retried, no fallback to another source). This keeps the core blocking and simple, needs no
  threads or fork (OS 9 has neither), and works the same on X and the Toolbox. Re-entrancy rule: while a
  fetch runs, the UI only repaints and records "refresh requested"/"quit"; it never starts a second fetch.
- **Entropy and time** (done): TLS takes both from the platform (`Net::now`, `Net::entropy`), not from `time()`
  and `/dev/urandom` directly; no entropy source means TLS refuses to start. Epoch times are 64-bit (`long` is 32-bit on ppc32).
  Known limit: `getaddrinfo()` is a blocking call and cannot yield, so a UI can freeze for the resolver's timeout. Needed for OS 9 (no /dev/urandom) and testable with a fake clock.
- **Settings store** (done: `SettingsStore`, posix dotfile): load/save of the raw text per OS, atomic save, follows a symlinked dotfile; the text editor `settings_update` is portable and refuses anything that would not read back (rc file on Unix, Preferences
  folder on OS 9). The *text* handling (parse, and "replace these keys but keep every other
  line and comment") is portable and tested; only reading/writing the bytes is per-OS.
- **Log sink**: stderr/syslog on Unix; a log file or window on OS 9.

## 3. The Model (portable, drives every UI; implemented in src/app/model.hpp)
State: Idle, Fetching, Ok(forecast, fetched_at), Error(status, last good forecast kept as Stale).
Inputs: `refresh()`, `tick(now)`, `settings_changed()`. Output: what to draw. Rules: auto-refresh every
15 min (Open-Meteo's update interval) when enabled; manual refresh always; failure backs off (1, 2, 4... up to
15 min) and keeps showing the last good data marked stale with its age; never fetches while one is running.
The UI is a thin renderer on top, so all refresh behaviour is tested natively with a fake clock and fake Net.

## 4. Targets and tiers

| target          | built on          | compiler                          | net / UI                         | package            | tier |
|-----------------|-------------------|-----------------------------------|----------------------------------|--------------------|------|
| host            | Fedora            | g++ 16, c++17                     | posix; CLI (+ X11 if available)  | none               | dev  |
| netbsd-macppc   | Fedora (cross)    | powerpc--netbsd-g++ 12.5, c++17   | posix; CLI + xweather (CDE look) | pkgin binary pkg   | 1    |
| macos9-ppc      | Fedora (Retro68)  | Retro68 GCC (version UNVERIFIED)  | Open Transport; Toolbox UI       | disk image / BinHex | 2    |
| tiger-ppc       | (parked)          | not planned                       |                                  |                    | -    |

**Scope (decided 2026-10-24): NetBSD and Mac OS 9 only.** Tiger is parked: the OS 9 app is expected to run in the
Classic environment on Tiger/PPC (UNVERIFIED that Classic is installed and working on maul's Tiger), which is also
the first place we test it before booting real OS 9. A native Tiger build is a possible later option (see below).

Tier 1 is gated by `make check` and `make smoke-maul`. Tier 2 may lag.

**Compiler gates (decide before starting each port):**
- Tiger (parked): Xcode 2.5's GCC is 4.0.1 (UNVERIFIED), which is C++98 and cannot build a C++17 `constexpr` core.
  If a native build is ever wanted: a modern GCC cross-compiler targeting powerpc-apple-darwin8 (old ld64/cctools that
  still support PPC, plus the 10.4u SDK from Xcode 2.5, Apple-licensed, never committed), feasibility UNVERIFIED; the
  alternatives (C++98 or C core, or a prebuilt modern GCC running on Tiger) were judged worse. Not on the roadmap.
- OS 9: check that Retro68's GCC accepts the core (C++17, `-fno-exceptions`) and that Open Transport is usable from it.
  A 20-line probe goes first. OS 9 memory is a fixed partition, so budget RAM (static buffers today: ~64 KB response,
  ~33 KB TLS) and set the SIZE resource deliberately.

## 5. Build system
- **GNU make is required** (reverses the earlier "BSD make too" goal: `include`/pattern rules differ and pkgsrc
  builds with gmake anyway). BSD hosts use `gmake`.
- Top-level `Makefile` + one fragment per target, `mk/<target>.mk`, chosen with `make TARGET=netbsd-macppc`.
  A fragment sets the toolchain, flags, platform sources and UI sources, and nothing else. Output goes to
  `build/<target>/`. Toolchain paths are variables with defaults, never hard-coded in sources.
- Targets that exist for every platform: `all check install dist clean`. `install` honours DESTDIR, PREFIX, MANDIR
  and CXX/CXXFLAGS/LDFLAGS (pkgsrc friendly). Target-specific: `smoke-maul`, `package`.
- Third-party code (BearSSL) is pinned (submodule or vendored at a commit) and built by our Makefile for each
  target; a variable allows an external one (pkgsrc's `security/bearssl`).
- Anything generated and committed (`anchors.h`) records how it was generated; regenerate with a make target.

## 6. Testing strategy
1. Build-time: `static_assert` in the core (also runs on big-endian targets because the compiler evaluates it).
2. Unit tests natively on the host, with sanitizers (`-fsanitize=address,undefined`) as a separate make target.
3. Feature tests: real binary against the fixture server (exit codes, stdout/stderr split, retries).
4. **Big-endian proof**: cross-build the unit test binary and run it on maul (`make check-maul`). This is the
   ground truth for endianness; a `powerpc-linux-gnu` + qemu-ppc build is only a possible extra (UNVERIFIED that
   Fedora packages a 32-bit big-endian PowerPC cross-compiler).
5. Smoke test on real hardware per Tier 1/2 target (`smoke-maul` now).
6. Later: fuzz the JSON/HTTP/config parsers on the host (libFuzzer or AFL), seeded from the fixtures.
Red/green rule: a bug found by any test or by hand gets a failing test first (json_get truncation was the first).

## 7. Release engineering
- Versioning: `MAJOR.MINOR.PATCH` single-sourced in the Makefile; git tag `vX.Y.Z`; `CHANGELOG` entry per release.
- `make dist` produces `weather-X.Y.Z.tar.gz` plus SHA-512 (becomes pkgsrc `distinfo`). Releases ship `LICENSE` and
  `THIRD_PARTY.md`.
- Reproducible builds: record toolchain versions per target (`docs/toolchains/<target>.md`, like netbsd-xc's
  README-repro), set `SOURCE_DATE_EPOCH`, use `-ffile-prefix-map`; two builds of a tag should match byte for byte
  (to be checked, not assumed).
- Symbols: the stripped binary ships; the `.debug` copy is archived with the release for core-file debugging.
- Budgets (tracked in CI output so growth is visible): CLI binary <= 200 KB stripped (153 KB now); no libstdc++.
- Warnings are errors for `core/` and `app/` on the host build; other code is warning-clean but not gated.
- Packaging: see PLAN (hand-built pkgin binary package first, pkgsrc source package later).

## 8. Source layout (done in M3a, 2026-10-24)
    src/core/      status text json http forecast provider openmeteo fetch config net log version (format is in forecast.hpp)
    src/app/       model (M3b)
    src/tls/       tls.cpp anchors.h
    src/platform/posix/   net_posix.cpp log_posix.cpp settings_posix.cpp
    src/cli/       main.cpp
    src/ui/xweather/      CDE-look Xlib widgets + main
    mk/            host.mk netbsd-macppc.mk (macos9.mk later)
    test/ docs/ spike/
