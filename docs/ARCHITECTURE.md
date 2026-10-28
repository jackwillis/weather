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
- **Net**: connect / close / sleep_ms / now / monotonic_ms, plus tls_open / tls_close.
- **Io**: read / write on a stream. Errors carry an `Err` kind, `errno`-style detail and a static message.
- **yield (new)**: `bool (*yield)(Net&)` called whenever the code would block (poll slices of ~50 ms,
  backoff sleeps). A UI uses it to pump its event loop (redraw "Fetching...", handle expose/close); returning
  false cancels the operation (`Err::Cancelled`, exit status 130, never retried, no fallback to another source). This keeps the core blocking and simple, needs no
  threads or fork (OS 9 has neither), and works the same on X and the Toolbox. Re-entrancy rule: while a
  fetch runs, the UI only repaints and records "refresh requested"/"quit"; it never starts a second fetch.
- **Entropy and time** (done): TLS takes both from the platform (`Net::now`, `Net::entropy`), not from `time()`
  and `/dev/urandom` directly; no entropy source means TLS refuses to start. Epoch times are 64-bit (`long` is 32-bit on ppc32).
  Known limit: `getaddrinfo()` is a blocking call and cannot yield, so a UI can freeze for the resolver's timeout. Needed for OS 9 (no /dev/urandom) and testable with a fake clock.
- **Attempt deadlines**: fetch uses nonnegative monotonic milliseconds, separate from UTC certificate time. POSIX exposes CLOCK_MONOTONIC; OS 9 uses the same modal-credit active clock as DNS and socket waits. Missing or regressing stream-clock samples fail closed. Reads and writes cannot start at or beyond the attempt limit. A scoped borrowed budget bounds POSIX platform waits and OS 9 async DNS; OS 9 stream-wait internals still use their per-wait timeout.
- **Settings store** (done: `SettingsStore`, posix dotfile): load/save of the raw text per OS, atomic save, follows a symlinked dotfile; the text editor `settings_update` is portable and refuses anything that would not read back (rc file on Unix, Preferences
  folder on OS 9). The *text* handling (parse, and "replace these keys but keep every other
  line and comment") is portable and tested; only reading/writing the bytes is per-OS.
- **Log sink**: stderr/syslog on Unix; a log file or window on OS 9.

## 3. The Model (portable, drives every UI; implemented in src/app/model.hpp)
State: Idle, Fetching, Ok(forecast, fetched_at), Error(status, last good forecast kept as Stale).
Inputs: `refresh()`, `tick(now)`, `settings_changed()`. Output: what to draw. Rules: auto-refresh every
15 min (Open-Meteo's update interval) when enabled; manual refresh requests within five seconds of the last fetch starting are ignored (never queued); automatic refresh and location changes bypass that floor; failure backs off (1, 2, 4... up to
15 min) and keeps showing the last good data marked stale with its age; never fetches while one is running.
The UI is a thin renderer on top, so all refresh behaviour is tested natively with a fake clock and fake Net.

### xweather (NetBSD UI), decisions of 2026-10-24
- Immediate-mode CDE-look primitives on plain Xlib (`ui/xweather/cde.*`), drawn into a Pixmap and copied to the window.
  No Xt, no libXm, and not API-compatible with Motif: the earlier "Motif-shaped stub API" idea was dropped because a
  read-only weather window needs about six primitives, and anything Motif-shaped would be a fiction we then had to maintain.
- One window. Menus, the Location dialog and the About box are overlays drawn inside it (no pop-up windows, no pointer
  grabs), so behaviour does not depend on the window manager and the same idea ports to OS 9.
- Colours derive from one background colour (CDE-like blue-grey), overridable with X resources. Fonts are X core
  fonts (Helvetica 12/24 from the 75dpi set, falling back to `fixed`).
- Fixed 440x300 window. Input handling during a fetch: repaint and quit only (re-entrancy rule above).
- GUI testing without a person: `xdrive` (synthetic events, screenshots) against a private Xvfb, on the laptop and on maul.

## Mac OS 9 target (decided 2026-10-25)

Built with Retro68 (GCC 16.1, PowerPC PEF, classic Toolbox, no Carbon), from `os9/CMakeLists.txt`; `make os9` runs it in the
Retro68 container. Layers: the same core, app and tls code; `src/platform/os9/` replaces `platform/posix/`.

* **Open Transport without Apple files.** Retro68 ships import libraries for Open Transport but no headers, and Apple's static
  glue (`OpenTransportAppPPC.o`) is not redistributable. We declare the API subset we use ourselves (`ot_min.h`, layouts and
  signatures verified against Apple's header and on real hardware) and replaced the glue with `ot_glue.cc`. The glue is tiny:
  a 40-byte "client record" whose address is the extra last argument of every `*Priv` call (Apple TN1173), set up once by
  `InitOpenTransportPriv(3, &record)`. Other Toolbox calls Retro68 lacks (`Microseconds`, `ReadLocation`) are in `toolbox_extra.h`.
* **Network I/O** (`net_os9.cpp`): endpoints are synchronous and non-blocking; "no data yet" is polled every 50 ms with
  `Net::yield`, so the event loop keeps running and Cmd-. cancels. DNS uses an asynchronous service provider with a host-tested
  controller (2026-10-25). The foreground loop yields and enforces a wrap-safe deadline. An abandoned open retains its callback
  storage until a later request drains it, or OT shuts down; a failed close quarantines the resolver until restart.
  POSIX `getaddrinfo` still blocks. Its elapsed time is charged when it returns,
  and all returned addresses share one connection timeout budget. A borrowed, scoped
  Net::budget also caps POSIX connect/read/write waits to the earlier attempt or caller
  limit; fetch restores the caller pointer on every return. OS9 async DNS also checks the borrowed attempt budget during open/query/drain;
  its TCP and stream waits do not yet enforce it internally (raw-I/O checks still apply). See `spike/os9/DNS-PROBE.md` for hardware coverage and limits.
* **Window scheduling**: SIZE allows background execution. The event pump checks osEvt suspend/resume flags and redraws on resume. A shared active-time clock excludes measured MenuSelect/DragWindow/TrackGoAway intervals from both DNS and socket/TLS deadlines, using modulo-32-bit tick arithmetic and an extended millisecond counter. This pauses only local deadline accounting; a server may close a connection during a long menu hold. New background/menu interaction acceptance is pending a Tiger boot; earlier window artwork/live-TLS scenarios passed under Classic.
* **Clock**: UTC is derived from the Date & Time time zone (`gmtDelta` plus the daylight-saving flag); TLS certificate dates
  use it, and `fetch` refuses a clock before 2025. Verified against OS X on the real machine: gmtDelta already includes daylight saving (an extra DST term made it an hour slow).
* **Entropy**: a unique secret seed provisioned from a modern OS CSPRNG initializes an in-process BearSSL HMAC_DRBG.
  Timing samples are supplemental only. An exclusive lock protects the Preferences seed store; its successor is saved through
  checked file exchange/volume flushes before output is released. Missing/invalid provisioning or failed rotation refuses TLS.
  No silent jitter-only fallback, no legacy seed promotion, and no guarantee of power-loss durability or compromise recovery.
  See `OS9-SEEDS.md` for setup, tests, recovery and remaining hardware verification.
* **Apple material policy**: nothing from Apple's SDK is committed or needed to build. Files obtained from Apple's old developer
  CDs for reading and comparison live outside the repo.

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
- OS 9, compile gate: PASSED 2026-10-24. Retro68's container image (`ghcr.io/autc04/retro68`, image id 7522dbcdad17) has
  GCC 16.1.0; it compiles the whole portable core, the app model and all compile-time tests with `-std=c++17
  -fno-exceptions -fno-rtti` for PowerPC *and* 68k, no warnings (`make probe-os9`). The link step works too.
- OS 9, networking gate: OPEN. Retro68's free "Multiversal Interfaces" do not include Open Transport or MacTCP (README and
  several projects agree); real OT builds use Apple's proprietary Universal Interfaces 3.4.x. Options: (a) jack supplies
  Universal Interfaces from his own copy of CodeWarrior/MPW (never committed); (b) declare the small OT subset we need
  (init, open endpoint, connect, send, receive, close, DNS lookup) by hand and generate import stubs with Retro68's
  MakeImport, then test in Classic on maul; (c) a MacTCP route via third-party code. Decide after a probe that links a PEF
  calling OTInitOpenTransport and runs in Classic.
  A 20-line probe goes first. OS 9 memory is a fixed partition, so budget RAM (static buffers today: ~64 KB response,
  ~33 KB TLS) and set the SIZE resource deliberately.

## Language level policy (decided 2026-10-24)
The core is C++17 and stays that way. The limiting factor for old systems is the compiler *available for them*, not the
OS: Retro68 gives GCC 16 for Mac OS 9 and even 68k System 7, and NetBSD/macppc has GCC 12.5, so every target in scope
accepts C++17 (probed, see above). Dropping to C++98 would cost the `constexpr` parsers and `static_assert` tests (the best
part of the test strategy), `string_view`, `enum class` and default member initialisers, and would be a near rewrite of
`core/` and `app/`. It would only be worth it for a target whose *only* compiler is old and which has no modern
cross-compiler (e.g. building natively on Tiger with Xcode 2.5's GCC 4.0.1, BeOS' gcc 2.95, IRIX/NeXT). If that day comes,
the layering makes a backport possible (the portable layers are lint-checked to be free of OS headers), but it would be a
second copy to maintain, so prefer finding or building a modern cross-compiler first.

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

## Native Preferences (PR #12)

The app/preferences.hpp policy selects nonempty Preferences before legacy weather.conf,
parses atomically, and commits UI changes only after a checked save. LocationDialog
uses app-owned DLOG/DITL resources and native TextEdit controls. Automatic model ticks
pause while editing; fields cannot open during a fetch. File Manager storage uses
private exclusive temporaries and checked flush/exchange/rename, with no physical
power-loss guarantee. Diagnostics explicitly supply isolated stores and are separate
executables. Cancel never clears an unresolved startup configuration error.
See OS9-PREFERENCES.md for migration and native verification gaps.

## Platform idioms and the pre-1.0 policy (decided 2026-10-28, #180)

Each platform's configuration, environment and file locations follow that platform's own idioms, as if the program had been designed only for it. The portable core owns one `Config` structure and knows nothing about files, flags, dialogs or the environment; each platform supplies a store (read and write) and a surface that edits it.

| Platform | Settings live in | Changed by | Environment |
|---|---|---|---|
| NetBSD and other BSDs | dotfile in `$HOME` | editing the text file; flags override | honor the system defaults (certificate variables, #158) |
| Linux (not supported yet) | `$XDG_CONFIG_HOME/<app>/config`, default `~/.config/<app>/config` | same text format and flags | XDG variables, system certificate store |
| macOS (not supported yet) | GUI app: `~/Library/Preferences`; CLI: dotfile | native Preferences window for the GUI app | system certificates |
| Mac OS 9 | one app-owned Preferences file in `System Folder:Preferences` | the Preferences window and menus only, never text editing | none; the app supplies what the OS lacks (TLS seed, #120) |

A setting may exist on only some platforms where a platform has no idiom for it; the core never depends on a platform-only setting. Linux and macOS are recorded here so the design does not close them off; nothing is implemented for them until they are supported.

**Pre-1.0 policy:** no migrations and no compatibility shims. Settings formats, file names, command names and paths may change between 0.x releases without a migration path. Remove old code paths instead of keeping them (for example the OS 9 `weather.conf` fallback and the text-editing instructions in OS9-PREFERENCES.md, which #182 removes). This policy ends at 1.0.

## OS 9 window cancellation and core events (#168/#169)

Command-period sets a per-fetch cancellation request, separate from terminal Quit.
The network yield returns false for either request; the existing fetch/model path
unwinds and retains previous data. Starting a later fetch clears only cancellation.

Window executables install core Apple Event handlers and advertise that support in
`os9/window.r`; console executables retain `os9/weather.r` without that flag. The
bounded window pump dispatches high-level events even while a Location dialog is
open or a fetch is yielding. A quit handler requests the same terminal Quit as the
menu; it never closes network/seed state from inside the callback. Once the fetch
has unwound, the window loop removes handlers and performs its normal terminal
network/seed shutdown. Handler-installation failure exits before network startup.
The window diagnostic sends actual quit events in separate idle/fetch runs and
checks lock availability after exit. Classic/native OS 9 acceptance remains pending.
