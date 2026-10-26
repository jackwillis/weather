# Big-endian CI

The supplemental gate cross-builds for 32-bit big-endian Linux/PPC and explicitly
runs the ABI probe, full unit suite and parser mutation suite through qemu-ppc.
UBSan uses trap mode, so undefined behavior fails without a target sanitizer
runtime. Run `make TARGET=linux-ppc check`; default loader prefix is
`/usr/powerpc-linux-gnu`, overridable with PPC_RUN.

This checks executed byte order and sizeof(long/pointer), not compiler macros.
Invoking the probe natively on x86-64 must fail. The full test suite still contains
the 32-bit overflow regressions that caught the earlier decimal parser bug.

## Scope and provenance

This is Linux syscall emulation, not NetBSD/macppc emulation. Real maul unit,
network, GUI, packaging and OS 9 checks remain separate acceptance requirements.
A full NetBSD image/toolchain CI environment remains future work; no personal
machine image, Apple SDK, maul keys or private TLS seeds are used here.

Ubuntu 24.04 distribution packages provide the compiler, target libc and QEMU.
APT verifies signed repository metadata and package hashes. Versions are recorded
in each job; security updates mean package versions are not bit-for-bit pinned.
Actions are pinned to commits and checkout credentials are not retained. The
workflow has read-only permissions, a 15-minute limit, two compiler jobs and no
privileged emulator registration. Package installation uses the disposable hosted
runner. Local validation uses an unprivileged container.

References:
- https://www.qemu.org/docs/master/user/index.html
- https://packages.ubuntu.com/noble/g%2B%2B-powerpc-linux-gnu

## Verification

Local Debian 12 container: GCC 12.2 and QEMU 7.2.22 executed 489 unit checks
and 206820 mutation inputs with zero failures. The runtime probe reported
bytes=1,2,3,4, long=4 and pointer=4; the same probe on the x86-64 host reported
bytes=4,3,2,1, long=8 and pointer=8 and correctly returned failure.

In an isolated copy of text.hpp, removing the integer overflow guard and parsing
999999999.0000 at four decimal places caused a QEMU trace/breakpoint trap under
UBSan. The valid 1.0 control completed first. No planted change entered the repo.
This validates overflow detection independently of compile-time assertions.

Hosted Ubuntu execution and its GCC 13/QEMU versions remain pending. The PR
stays draft until hosted results are recorded. No claim of NetBSD syscall or
hardware acceptance is made. Full-system NetBSD selection remains deferred.

Host regression: make check passed 492 unit checks, 206820 mutation inputs,
99 CLI checks and 21 GUI checks; ASan/UBSan checks passed. An initial sandboxed
run failed at socket creation (test/unittest.cpp:375, ls >= 0); rerunning with
local fixture networking available passed. The emulated suite passed twice.
