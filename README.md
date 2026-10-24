# weather

A small Unix weather tool for old big-endian machines. It reads forecasts over TLS (BearSSL),
works without a proxy, and is cross-built from Linux for NetBSD/macppc first, then Mac OS 9.
The portable core (`src/`, no OS headers except in `net_posix.cpp`/`log.cpp`) is separate from the
per-OS UI layer. NetBSD gets `xweather`, a CDE-look window drawn in plain Xlib (no Xt, no Motif).

Status: the CLI (`weather`) and `xweather` work on maul. `weather -1` prints a live forecast; `xweather` shows it in a window
(menus, Location dialog, auto-refresh every 15 min, weather pictures). Not yet run by a person on a real display.

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
Config: `~/.weatherrc` (`latitude`, `longitude`, `units`, `timeout`, `attempts`, `providers`, `log`),
flags override it. The default location is Madison, WI 53703 (Capitol Square 43.0747,-89.3841); set `name`, `latitude`, `longitude` or use -l/-n. Exit codes follow BSD sysexits
(68 dns, 69 connect/no source, 74 io, 75 timeout, 76 http/tls/cert, 65 bad data, 64 usage, 78 config).
`-S` saves location, name and units into the config file (comments and other settings are kept; the write is atomic) and exits. Ctrl-C or SIGTERM during a fetch cancels cleanly (exit 130). Diagnostics go to stderr (or syslog with -s); stdout carries only the result. `-d` dumps the traffic.

## Build and test
GNU make is required (`gmake` on BSD). One fragment per target in `mk/`.

    make                          # native build (build/host/weather), -Werror
    make check                    # lint + unit tests (static_assert + runtime) + feature tests (fixture server)
    make lint                     # portable layers (src/core, src/app) may include only freestanding headers
    make TARGET=netbsd-macppc     # cross-build for NetBSD/macppc (build/netbsd-macppc/weather, stripped; .debug keeps symbols)
    make ppc                      # same as TARGET=netbsd-macppc
    make check-maul               # cross-build the unit tests and run them on maul (big-endian ground truth)
    make smoke-maul               # cross-build, push to maul, test the real binary there
    make check-gui                # xweather in a private Xvfb on this machine, with screenshots in build/gui-host/
    make check-gui-maul           # the same scenario with the real PowerPC xweather in Xvfb on maul (build/gui/)
    ONLINE=1 sh test/feature.sh build/host/weather   # also test against the real server

Needs (xweather, host build): libX11-devel; for `check-gui`: Xvfb and ImageMagick. A BearSSL checkout at `spike/bearssl` (git clone https://www.bearssl.org/git/BearSSL; gitignored, a
pinned copy comes later; the Makefile builds its library per target), python3 (fixture server only), and for
the cross target the toolchain from `~/scratch/netbsd-xc` (see its README-repro.md; paths are variables in
`mk/netbsd-macppc.mk`). Trust anchors (`src/tls/anchors.h`) are generated with `brssl ta`; refreshing them is
a release task.

## Layout (dependencies point down only; see docs/ARCHITECTURE.md)
- `src/core/` portable, constexpr, no OS headers: status, text, json, http, forecast, provider, openmeteo, fetch, config, log/net interfaces
- `src/app/` portable app logic: Model (phases, 15 min auto-refresh, backoff, stale data, status line), TextEdit, LocationForm
- `src/ui/xweather/` the X11 window: CDE drawing primitives (`cde.*`, weather art) and the app (`main.cpp`)
- `src/tls/` BearSSL glue + trust anchors
- `src/platform/posix/` sockets with yield/cancel, clock, entropy, settings dotfile store, syslog/stderr log sink, libstdc++ shim
- `src/cli/` the `weather` command
- `mk/` per-target build fragments; `test/` unit tests, fixtures, fixture server, feature and maul smoke scripts; `spike/` experiments

## License
BSD 2-clause (`LICENSE`). BearSSL (MIT) is linked statically; its notice is in `THIRD_PARTY.md`.
