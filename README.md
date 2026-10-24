# weather

A small Unix weather tool for old big-endian machines. It reads forecasts over TLS,
works without a proxy, and is built for NetBSD/macppc first, then Mac OS 9 and Tiger.
The portable core is separate from the per-OS UI layer. NetBSD gets a Motif-style Xlib window.

Status: just started. Nothing here works yet.

## Cross-toolchain (kept outside this repo)
NetBSD 11.0 macppc cross-compiler and sysroot live in `~/scratch/netbsd-xc`
(see its `README-repro.md` for how they were built).

    CC=~/scratch/netbsd-xc/tools2/bin/powerpc--netbsd-gcc
    SYSROOT=~/scratch/netbsd-xc/sysroot

## Layout
- `docs/PLAN.md` goals, quality bar, milestones, decision log; `TODO.md` task list
- `spike/` throwaway experiments (C++17 check; BearSSL TLS test)
