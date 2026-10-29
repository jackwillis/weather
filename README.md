# What It Be Like?

<p align="center"><img src="assets/logo/whatitbelike-512x256.png" alt="What It Be Like? logo" width="384"></p>

*A little weather application for old computers.*

What It Be Like? brings live weather forecasts to classic Macintosh and Unix-like systems.
It connects directly to weather services over HTTPS, displays conditions in native
interfaces, and can save the last forecast for use when the network is unavailable.
The programs share a portable C++17 core with separate operating-system interfaces:

- **Weather** — a native Mac OS 9 window application, built with Retro68 and the classic Macintosh Toolbox.
- **xweather** — a CDE-inspired X11 weather window for NetBSD, implemented in plain Xlib.
- **weather** — a Unix command-line client.

The native CLI also builds and runs on Linux and modern macOS, but the project's
Unix package is currently built for **NetBSD/macppc only**. Development and hardware
acceptance focus on a Power Mac G4 running NetBSD/macppc, Mac OS 9.2.2 and
Mac OS X 10.4 Tiger's Classic environment.

**Download:** [v0.0.1 prerelease](https://github.com/jackwillis/weather/releases/tag/v0.0.1)
([release notes](docs/release-notes/0.0.1.md)). This early release primarily
demonstrates the packaging and deployment pipeline. **v0.1.0 is in development.**
The version in this tree is unreleased; check [docs/STATUS.md](docs/STATUS.md)
for the latest test evidence and [the v0.1 release tracker](https://github.com/jackwillis/weather/issues/45)
for the remaining acceptance work.

## Platforms and readiness

For v0.1 the supported release targets are **NetBSD/macppc** (one combined CLI/X11
package) and **PowerPC Mac OS 9.2.2**. Tiger Classic is an additional validation
environment. Linux and modern macOS are development hosts; this is not a promise
of native Tiger, Linux or macOS release packages. Final hardware acceptance is
tracked in [#204](https://github.com/jackwillis/weather/issues/204).

The approved Unix rename to `belike`, `xbelike` and `~/.belikerc` is pending
[#135](https://github.com/jackwillis/weather/issues/135); commands below describe
the current source. There will be no old-name alias or rc migration. Native OS 9
`Weather`, its Preferences and TLS seed/lock names stay unchanged.

| Platform | Application | Current evidence |
| --- | --- | --- |
| NetBSD/macppc on Power Mac G4 | `weather`, `xweather` | Currently packaged Unix target; live CLI and X11/Xvfb hardware tests; real-display review of xweather remains open (#70). |
| Linux | `weather` (native build); `xweather` when X11 dependencies are available | Builds and runs from source; no Linux distribution package supplied. |
| Modern macOS | `weather` (native build); `xweather` prospective via XQuartz | CLI builds and runs from source; X11 GUI build/runtime on XQuartz has **not yet been verified** ([#171](https://github.com/jackwillis/weather/issues/171)). No macOS package supplied. |
| Mac OS 9.2.2 (native) | `Weather` | Owner-observed working production application across restart; full native acceptance remains open (#102). |
| Mac OS 9 under Tiger Classic | `Weather` | Live verified-TLS fetches, window diagnostic and cache-recovery tests passed; remaining lifecycle and Preferences checks are tracked. |

### Screenshots

Screenshots from actual Mac OS 9 and NetBSD/X11 runs will be added after
selection and review; the artwork in this README is the project logo, not a UI mockup.

### Before installing

The OS 9 application currently requires a unique TLS seed provisioned separately
with `SeedInstall` ([instructions and recovery](docs/OS9-SEEDS.md)).
Stand-alone first-run entropy collection is planned for v0.1 (#120), but remains
dependent on the measurements and review in #121. Do not treat ordinary mouse
movement as proven cryptographic entropy.

In the OS 9 window app, Command-period does **not** yet cancel an in-progress fetch
(#168), and system-requested quit can block logout or Classic shutdown (#169).
Use File > Quit or Command-Q to quit manually. See the release notes for limitations
of the published prerelease; these issue links describe ongoing development.

### Quick look

    $ weather -1
    54.6F Clear sky, wind 9.0mph SE, humidity 83% (open-meteo 2026-10-24T05:00)

The example above is illustrative, not a live forecast.

For project goals and technical detail, see [docs/PLAN.md](docs/PLAN.md),
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), and [the issue tracker](https://github.com/jackwillis/weather/issues).

Place-name lookup is explicit and read-only: `weather -g "Madison, WI"` lists
up to five matches with region/country and coordinates, then exits. Choose the
coordinates deliberately and use a separate command such as
`weather -l 43.0731,-89.4012 -n Madison -S` to save. Search credits Open-Meteo /
GeoNames; its public endpoint is for non-commercial use. With missing or empty
settings, the window shows the default location (Madison, WI) and fetches it;
Location... (Ctrl-L) changes it. The CLI remains non-interactive. See
[search behavior and privacy](docs/GEOCODING.md). X11 Location has Find... (Ctrl-F) with explicit result selection; OK saves
separately. OS 9 search controls are integrated; native/Classic acceptance remains in #204.

## xweather
    xweather [-dhqvV] [-c file] [-display name]
A CDE-look window: menu bar (File: Refresh F5, Location... Ctrl+L, Exit Ctrl+Q; Options: auto-refresh, units and forecast saving; Help: About xweather... > Credits...),
a picture for the conditions (sun, moon, clouds, rain, snow, storm, fog), temperature, wind, humidity, and a status bar
("Updated 5 min ago / Next update in 10 min"; after a failure it keeps showing the last good data, marked as stale).
It shares `~/.weatherrc` with `weather`. Closing the window during a fetch cancels it. On modern macOS, XQuartz may provide an X11 server and development libraries, but `xweather` compatibility has not yet been verified; see [#171](https://github.com/jackwillis/weather/issues/171). X resources (xrdb):
`xweather.background`, `.foreground`, `.font`, `.boldFont`, `.bigFont`.

## Use
    weather [-1dhqsSV] [-c file] [-l lat,lon] [-n name] [-p sources] [-t secs] [-u metric|imperial] [-v] [-g query]
Data sources: Open-Meteo first, then MET Norway automatically if it fails. `nws` (US station observations; see [NWS notes](docs/NWS.md)) can be added with `-p` or `providers`. MET identifies the app using `https://github.com/jackwillis/weather`; an optional `contact` overrides that URL. Existing `providers` settings and `-p` keep their explicit order. MET requires forecast saving; with saving off, it is skipped. Saved MET responses are reused until the service’s `Expires` time, then conditionally revalidated.

Attribution: weather data comes from [Open-Meteo](https://open-meteo.com) and, if enabled, the [MET Norway Locationforecast API](https://api.met.no), both under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); the values are converted to your units and summarised, otherwise unchanged. The `nws` source (if enabled) uses National Weather Service (NOAA) U.S. public-domain observations; values are converted and summarised, with no NOAA/NWS endorsement or affiliation. See [NWS terms](https://www.weather.gov/disclaimer) and `docs/NWS.md`. Open-Meteo's free tier is for non-commercial use. See `docs/PROVIDER-TERMS.md`.
Config: `~/.weatherrc` (`latitude`, `longitude`, `name`, `units`, `timeout`, `attempts`, `providers`, `contact`, `log`, `autorefresh`),
flags override it. The default location is Madison, WI 53703 (Capitol Square 43.0747,-89.3841); set `name`, `latitude`, `longitude` or use -l/-n. Exit codes follow BSD sysexits
(68 dns, 69 connect/no source, 74 io, 75 timeout, 76 http/tls/cert, 65 bad data, 64 usage, 78 config).
When all sources fail, the last attempted source supplies the error and exit code, including with `-q`; later skipped sources do not hide it.
`-S` saves location, name and units into the config file (comments and other settings are kept; the write is atomic) and exits. Ctrl-C or SIGTERM during a fetch cancels cleanly (exit 130). Diagnostics go to stderr (or syslog with -s); stdout carries only the result. `-d` dumps the traffic.

Forecast saving is on by default. Both window apps offer **Save forecasts on this
computer** in Options. Saved forecasts appear clearly dated while an update is
attempted; CLI fallback remains explicitly stale and retains the live failure exit
code. Turning saving off (`cache = off`) removes the saved forecast and prevents
further forecast storage. MET Norway requires saving; choose Open-Meteo to use
Weather without persistent forecasts.
Plain-HTTP test requests bypass caching. See
[cache behavior and format](docs/FORECAST-CACHE.md). OS 9 forecast caching has been exercised under Classic and owner-observed under native OS 9; the saving-off flow and full native acceptance remain pending.

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
Some native Preferences hardware acceptance remains pending.

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
