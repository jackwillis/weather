# weather

A small Unix weather tool for old big-endian machines. It reads forecasts over TLS (BearSSL),
works without a proxy, and is cross-built from Linux for NetBSD/macppc first, then Mac OS 9.
The portable core (`src/`, no OS headers except in `net_posix.cpp`/`log.cpp`) is separate from the
per-OS UI layer. NetBSD will get a Motif-style window drawn in plain Xlib (milestone 3).

Status: M1 (TLS) and M2 (core + CLI) work. Running on maul: `weather -1` prints a live forecast.

    $ weather -1
    54.6F Clear sky, wind 9.0mph SE, humidity 83% (open-meteo 2026-10-24T05:00)

See `docs/PLAN.md` (goals, milestones, decision log), `docs/ARCHITECTURE.md` (layers, seams, targets, build, tests, releases) and `TODO.md`.

## Use
    weather [-1dhqsV] [-c file] [-l lat,lon] [-p sources] [-t secs] [-u metric|imperial] [-v]
Config: `~/.weatherrc` (`latitude`, `longitude`, `units`, `timeout`, `attempts`, `providers`, `log`),
flags override it. The default location is Madison, WI 53703 (Capitol Square 43.0747,-89.3841); set `name`, `latitude`, `longitude` or use -l/-n. Exit codes follow BSD sysexits
(68 dns, 69 connect/no source, 74 io, 75 timeout, 76 http/tls/cert, 65 bad data, 64 usage, 78 config).
Diagnostics go to stderr (or syslog with -s); stdout carries only the result. `-d` dumps the traffic.

## Build and test
GNU make is required (`gmake` on BSD). One fragment per target in `mk/`.

    make                          # native build (build/host/weather), -Werror
    make check                    # lint + unit tests (static_assert + runtime) + feature tests (fixture server)
    make lint                     # portable layers (src/core, src/app) may include only freestanding headers
    make TARGET=netbsd-macppc     # cross-build for NetBSD/macppc (build/netbsd-macppc/weather, stripped; .debug keeps symbols)
    make ppc                      # same as TARGET=netbsd-macppc
    make smoke-maul               # cross-build, push to maul, test the real binary there
    ONLINE=1 sh test/feature.sh build/host/weather   # also test against the real server

Needs: a BearSSL checkout at `spike/bearssl` (git clone https://www.bearssl.org/git/BearSSL; gitignored, a
pinned copy comes later; the Makefile builds its library per target), python3 (fixture server only), and for
the cross target the toolchain from `~/scratch/netbsd-xc` (see its README-repro.md; paths are variables in
`mk/netbsd-macppc.mk`). Trust anchors (`src/tls/anchors.h`) are generated with `brssl ta`; refreshing them is
a release task.

## Layout (dependencies point down only; see docs/ARCHITECTURE.md)
- `src/core/` portable, constexpr, no OS headers: status, text, json, http, forecast, provider, openmeteo, fetch, config, log/net interfaces
- `src/app/` portable app model (M3b)
- `src/tls/` BearSSL glue + trust anchors
- `src/platform/posix/` sockets, clock, syslog/stderr log sink, libstdc++ shim
- `src/cli/` the `weather` command
- `mk/` per-target build fragments; `test/` unit tests, fixtures, fixture server, feature and maul smoke scripts; `spike/` experiments

## License
BSD 2-clause (`LICENSE`). BearSSL (MIT) is linked statically; its notice is in `THIRD_PARTY.md`.
