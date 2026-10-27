#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Objects must rebuild when the compiler command changes, and only then (the .flags rule in the Makefile).
# Builds one object into a scratch dir with the make that invoked us.  usage: sh test/flags.sh MAKE
MAKE_CMD=${1:?usage: flags.sh make-command}
pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "FAIL: $1"; }

T=
trap 'rm -rf "$T"' EXIT
trap 'exit 1' INT TERM
T=$(mktemp -d "${TMPDIR:-/tmp}/flags-test.XXXXXX") || exit 1
OBJ=$T/obj/core/fetch.o
# MAKEFLAGS is cleared so an outer -s/-n/-j (inherited through make's environment) cannot hide or change the output.
build() { OUT=$(MAKEFLAGS= $MAKE_CMD BUILD="$T" "$@" "$OBJ" 2>&1); got=$?; }
# GNU make 3.81 compares file times in whole seconds: let the clock tick before changing flags.
tick() { sleep 1; }
compiled() { case "$OUT" in *"-c -o "*) return 0;; *) return 1;; esac; }

build
if [ "$got" = 0 ] && [ -f "$OBJ" ] && compiled; then ok; else bad "first build: exit $got ($OUT)"; fi

build
if [ "$got" = 0 ] && ! compiled; then ok; else bad "unchanged flags rebuilt (exit $got): $OUT"; fi

tick; build CXXFLAGS=-DFLAGS_TEST=1
if [ "$got" = 0 ] && compiled; then ok; else bad "changed flags did not rebuild (exit $got): $OUT"; fi
case "$OUT" in *"-DFLAGS_TEST=1"*) ok;; *) bad "changed flags not on the compile line: $OUT";; esac

build CXXFLAGS=-DFLAGS_TEST=1
if [ "$got" = 0 ] && ! compiled; then ok; else bad "same changed flags rebuilt (exit $got): $OUT"; fi

tick; build
if [ "$got" = 0 ] && compiled; then ok; else bad "reverting flags did not rebuild (exit $got): $OUT"; fi

echo "flags tests: $pass passed, $fail failed"
[ "$fail" = 0 ]
