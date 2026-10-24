#!/bin/sh
# Does Retro68's compiler (the OS 9 toolchain) accept the portable core and pass every compile-time test?
# Uses Retro68's published container image (rootless podman or docker); nothing is installed on the host.
# usage: tools/probe-os9.sh        (compile-only, PowerPC and 68k)
cd "$(dirname "$0")/.." || exit 1
RT=$(command -v podman || command -v docker) || { echo "probe-os9: needs podman or docker"; exit 2; }
IMG=ghcr.io/autc04/retro68:latest
VERSION=$(sed -n 's/^VERSION *= *//p' Makefile)
fail=0
for T in powerpc m68k; do
	out=$($RT run --rm -v "$PWD":/src:Z -w /src $IMG sh -c "
	    /Retro68-build/toolchain/bin/$T-apple-macos-g++ -std=c++17 -O2 -Wall -Wextra -fno-exceptions -fno-rtti \
	    -DWEATHER_VERSION='\"$VERSION\"' -Isrc -c spike/os9probe.cpp -o /tmp/probe.o 2>&1; echo STATUS=\$?; \
	    /Retro68-build/toolchain/bin/$T-apple-macos-g++ --version | head -n 1" 2>&1 | grep -v 'Using multiversal')
	case "$out" in
	*STATUS=0*) echo "ok   $T: portable core + compile-time tests compile ($(echo "$out" | tail -n 1))" ;;
	*) echo "FAIL $T:"; echo "$out"; fail=1 ;;
	esac
done
exit $fail
