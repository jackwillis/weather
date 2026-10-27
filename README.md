# What It Be Like?

<p align="center"><img src="assets/logo/whatitbelike-512x256.png" alt="What It Be Like? logo" width="384"></p>

*What It Be Like?* is the project; its programs are `weather` (command line), `xweather` (NetBSD/X11 window) and `Weather` (Mac OS 9 window).
A small weather tool for old big-endian machines. It reads forecasts over TLS (BearSSL),
works without a proxy, and is cross-built from Linux for NetBSD/macppc first, then Mac OS 9.
The portable core (`src/`, no OS headers except in `net_posix.cpp`/`log.cpp`) is separate from the
per-OS UI layer. NetBSD gets `xweather`, a CDE-look window drawn in plain Xlib (no Xt, no Motif).

**Releases:** https://github.com/jackwillis/weather/releases. 0.0.1 is a deliberately rough pre-release that proves the release pipeline (see `docs/release-notes/0.0.1.md`); the version in this tree is the unreleased 0.1.0.

Status (details in `docs/STATUS.md`): the CLI (`weather`) and `xweather` work on maul (NetBSD/macppc). A first Mac OS 9 build
(`Weather`) has a native window; `WeatherConsole` retains the console diagnostic.
`weather -1` prints a live forecast; `xweather` shows it in a window (menus, Location dialog, auto-refresh every 15 min, weather pictures).

    $ weather -1
    54.6F Clear sky, wind 9.0mph SE, humidity 83% (open-meteo 2026-10-24T05:00)

See `docs/PLAN.md` (goals, milestones, decision log), `docs/ARCHITECTURE.md` (layers, seams, targets, build, tests, releases) and `TODO.md`.

## xweather
    xweather [-dhqvV] [-c file] [-display name]
A CDE-look window: menu bar (File: Refresh F5, Location... Ctrl+L, Exit Ctrl+Q; Options: auto-refresh, units; Help: About),
a picture for the conditions (sun, moon, clouds, rain, snow, storm, fog), temperature, wind, humidity, and a status bar
("Updated 5 min ago / Next update in 10 min"; after a failure it keeps showing the last good data, marked as stale).
It shares `~/.weatherrc` with `weather`. Closing the window during a fetch cancels it. X resources (xrdb):
`xweather.background`, `.foreground`, `.font`, `.boldFont`, `.bigFont`.

## Use
    weather [-1dhqsSV] [-c file] [-l lat,lon] [-n name] [-p sources] [-t secs] [-u metric|imperial] [-v]
Data sources: `open-meteo` (default), `nws` (US station observations; see [NWS notes](docs/NWS.md)) and `met-no` (MET Norway; needs `contact = ...` in the config because their terms require it, skipped otherwise); sources are tried in the order given by `-p`/`providers`.

Attribution: weather data comes from [Open-Meteo](https://open-meteo.com) and, if enabled, the [MET Norway Locationforecast API](https://api.met.no), both under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); the values are converted to your units and summarised, otherwise unchanged. The `nws` source (if enabled) uses the US National Weather Service API; see `docs/NWS.md`. Open-Meteo's free tier is for non-commercial use. See `docs/PROVIDER-TERMS.md`.
Config: `~/.weatherrc` (`latitude`, `longitude`, `name`, `units`, `timeout`, `attempts`, `providers`, `contact`, `log`, `autorefresh`),
flags override it. The default location is Madison, WI 53703 (Capitol Square 43.0747,-89.3841); set `name`, `latitude`, `longitude` or use -l/-n. Exit codes follow BSD sysexits
(68 dns, 69 connect/no source, 74 io, 75 timeout, 76 http/tls/cert, 65 bad data, 64 usage, 78 config).
`-S` saves location, name and units into the config file (comments and other settings are kept; the write is atomic) and exits. Ctrl-C or SIGTERM during a fetch cancels cleanly (exit 130). Diagnostics go to stderr (or syslog with -s); stdout carries only the result. `-d` dumps the traffic.

## Build and test
GNU make is required (`gmake` on BSD and under Homebrew; Apple's make 3.81 works too). One fragment per target in `mk/`.

    make                          # native build (build/host/weather), -Werror
    make check                    # lint, anchors (+ test/anchors.sh), unit (static_assert + runtime), robustness, CLI feature tests (fixture server),
                                  # seed provisioning, Makefile flags test; plus the GUI scenario when X11 headers are present
    make lint                     # portable layers (src/core, src/app) may include only freestanding headers
    make TARGET=netbsd-macppc     # cross-build for NetBSD/macppc (build/netbsd-macppc/weather, stripped; .debug keeps symbols)
    make ppc                      # same as TARGET=netbsd-macppc
    make check-maul               # cross-build the unit and robustness tests and run them on maul (big-endian ground truth)
    make smoke-maul               # cross-build, push to maul, test the real binary there
    make check-san                # unit + robustness + feature tests with UBSan (trap mode; ASan too if libasan is installed)
    make check-tls                # certificate verification, offline: real tls.cpp vs a local TLS server with generated certificates (openssl, python3)
    make check-gui                # xweather in a private Xvfb on this machine, with screenshots in build/gui-host/
    make check-gui-maul           # the same scenario with the real PowerPC xweather in Xvfb on maul (build/gui/)
    ONLINE=1 sh test/feature.sh build/host/weather   # also test against the real server

Needs: a C++17 compiler and GNU make; BearSSL is vendored in `third_party/bearssl` (pinned, built per target by the Makefile).
GitHub Actions runs host/GUI checks and ASan/UBSan sequentially, plus a separate 32-bit unit/mutation/CLI job.
CI requires the ASan runtime (`REQUIRE_ASAN=1`) and fails rather than silently downgrading to UBSan-only checks.
The 32-bit job can be reproduced with `make TARGET=host-32 check` on x86 Linux with g++ multilib installed.
CI uses fixtures and no private seeds or maul credentials; real PowerPC and OS 9 checks remain separate hardware gates.
Optional: libX11-devel (builds `xweather`), Xvfb + ImageMagick (`check-gui`), python3 (fixture servers for tests only), and for
the cross target the toolchain from `~/scratch/netbsd-xc` (see its README-repro.md; paths are variables in
`mk/netbsd-macppc.mk`). Trust anchors: `src/tls/anchors.h` is generated from `src/tls/roots.pem` (`make anchors`) and `make check` verifies
the header matches the PEM and that the PEM holds exactly ISRG Root X1, ISRG Root X2 and HARICA TLS RSA Root CA 2021 (checked by SHA-256
fingerprint; `test/anchors.sh` tests the check itself). Refreshing them is a release task.

Project tracking (issues, milestones, labels, the board, PR flow): see [docs/PROJECT-MANAGEMENT.md](docs/PROJECT-MANAGEMENT.md).

### Host platforms
Developed on Linux; `make check` (everything except the GUI scenario) also passes on macOS (arm64) with Apple's GNU make 3.81 and with
Homebrew's `make` 4.x (installed as `gmake`); `make dist distcheck` passes there too. Besides the compiler: `openssl` (the anchor fingerprint check; without it that check is
skipped with a notice), `python3` (fixture servers, seed tests) and POSIX `sh`.

Linux only (or needing extra tools): `check-gui` (Xvfb; `mk/host.mk` looks for `/usr/include/X11`, so it is skipped on macOS), AddressSanitizer in
`check-san` (detected by asking the compiler for `libasan.so`; macOS gets UBSan only), `make os9` (podman) and the `*-maul` targets (the test machine).

Shell scripts in `tools/` and `test/` are POSIX `sh` and POSIX `awk`: some run on NetBSD, whose awk is BSD awk, so no gawk-only idioms. macOS's
awk is BSD awk too, which makes it a useful check. With GNU make 3.81, flags changed within one second of the previous build may not
rebuild objects (3.81 compares times in whole seconds); use `make clean` after scripted flag changes.

## Mac OS 9
    make os9      # build/os9/Weather.{APPL,bin,dsk}: PowerPC PEF app, built with Retro68 in a podman container, no Apple files needed
`Weather` shows a native window with weather artwork, forecast values, source and
observation time, and refresh/stale-data status. File/Refresh (Cmd-R), File/Quit
(Cmd-Q), Options/automatic updates and units, and Apple/About are available.
During a fetch, File/Quit, Cmd-Q or the close box cancels. Menus and dragging remain
available; their modal tracking time is excluded from local DNS and socket/TLS
deadlines. A server can still close an idle connection during a long hold. The app
continues fetching and refreshing in the background. Options/Location (Cmd-L) opens a native validated editor. Location, units and
automatic-update changes save to Preferences before taking effect; Apply/Cancel
and failure behavior are described in [OS 9 Preferences](docs/OS9-PREFERENCES.md).
Native Preferences hardware acceptance is still pending.

Both apps prefer nonempty `Weather Preferences` in the classic Preferences folder,
otherwise optional `weather.conf` next to the app (same `key = value` lines as
`~/.weatherrc`, at most 4095 bytes). Missing files use defaults; unreadable or invalid
files report an error without applying partial changes. The first successful UI
save preserves legacy settings/comments in Preferences and leaves weather.conf intact. `WeatherConsole` fetches,
prints and waits for a click; Cmd-. cancels its fetch. Debug output is appended to
`weather.log`; the managed TLS seed lives in Preferences.
Needs Open Transport with TCP/IP configured. The network layer (`src/platform/os9/`) talks to Open Transport
through our own small declarations and client glue (see docs/ARCHITECTURE.md, "Mac OS 9 target"). DNS uses an asynchronous provider with timeout/cancel support.
TLS requires a unique provisioned seed; `SeedInstall` installs it in Preferences and the app checks seed rotation before releasing entropy.
See [OS 9 seed setup and recovery](docs/OS9-SEEDS.md). Legacy jitter-only seeds are not imported. Retro68's `printf` still lacks `%lld`.
To run it on a Mac with OS X (Classic) use `tools/deliver-os9.sh` (see its comments) and open the app.

Optional diagnostics: configure the OS 9 CMake build with
`-DWEATHER_BUILD_DIAGNOSTICS=ON`, build and deliver `WeatherUITest.dsk`, then run
`python3 tools/check-os9-window.py /tmp/weather-window-shots` from the laptop.
The driver uses explicit marker files in the separate test app, takes screenshots,
and verifies a yielding update, refresh suppression, units, stale data, live TLS and
quit. It also activates Finder programmatically to test background execution and
suspend/resume; it never clicks or answers a system dialog. Production `Weather` does not link fixture/marker code. The test app has a
ten-minute watchdog and never answers system dialogs or changes the user's config.

## Install and packages
    make install DESTDIR=/stage PREFIX=/usr/pkg MANDIR=/usr/pkg/man   # bin/weather, bin/xweather, man pages, example config
    make plist PREFIX=/usr/pkg MANDIR=/usr/pkg/man                    # the file list (a package PLIST)
    make dist                                                         # build/dist/weather-VERSION.tar.gz + SHA-512 (clean tree, reproducible)
    make distcheck                                                    # unpack the tarball elsewhere, build, test, rebuild it and compare bytes
    make package-netbsd                                               # build/pkg/weather-VERSION.tgz, a pkg_add-able NetBSD package
NetBSD package: `tools/mkpkg-netbsd.sh` cross-builds, stages `make install`, and runs `pkg_create` on maul (it only archives
files). Install on maul with `doas pkg_add weather-0.1.0.tgz`; remove with `pkg_delete weather`. No install script is needed:
pkg_add installs exactly the PLIST and settings are a per-user dotfile.

## Layout (dependencies point down only; see docs/ARCHITECTURE.md)
- `src/core/` portable, constexpr, no OS headers: status, text, json, http, forecast, provider, openmeteo, fetch, config, log/net interfaces
- `src/app/` portable app logic: Model (phases, 15 min auto-refresh, backoff, stale data, status line), TextEdit, LocationForm
- `src/ui/xweather/` the X11 window: CDE drawing primitives (`cde.*`, weather art) and the app (`main.cpp`)
- `src/tls/` BearSSL glue + trust anchors
- `src/platform/posix/` sockets with yield/cancel, clock, entropy, settings dotfile store, syslog/stderr log sink, libstdc++ shim
- `src/platform/os9/` Mac OS 9: Open Transport network layer, Open Transport declarations and glue, console app and log sink; `os9/` is its CMake build
- `src/cli/` the `weather` command
- `third_party/bearssl/` pinned BearSSL; `tools/` anchor generation and check
- `mk/` per-target build fragments; `test/` unit tests, fixtures, fixture server, feature and maul smoke scripts; `spike/` experiments

## License
GNU General Public License, version 3 or (at your option) any later version (`LICENSE`). Copyright (c) 2026 Jack Willis.
BearSSL (MIT) is linked statically, and the Mac OS 9 build links the Retro68 runtime (GPL-3.0+ with the GCC runtime
exception) and the Multiversal Interfaces (MIT); their notices are in `THIRD_PARTY.md`. Versions before the GPL change
were published under the BSD 2-clause licence.
