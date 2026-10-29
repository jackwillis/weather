#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Push the cross-built binary to maul (NetBSD) and check it for real.
# Needs maulctl (see the maul notes) and an up NetBSD session on maul.
BIN=${1:-build/netbsd-macppc/belike}
TEST_BIN=${2:?usage: smoke-maul.sh PRODUCTION TEST_HOOKS}
fail=0
check() { if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: got '$2' wanted '$3'"; fail=1; fi; }

maulctl push nb "$BIN" belike >/dev/null || { echo "cannot push to maul"; exit 1; }
maulctl push nb "$TEST_BIN" belike-test-hooks >/dev/null || exit 1
maulctl nb 'chmod 755 /tmp/belike /tmp/belike-test-hooks'
check "file type" "$(file "$BIN" | sed 's/.*ELF 32-bit MSB.*PowerPC.*NetBSD.*/macppc-ok/;t;s/.*/WRONG/')" macppc-ok
check "no libstdc++" "$(readelf -d "$BIN" 2>/dev/null | grep -c libstdc || true)" 0
check "version" "$(maulctl nb '/tmp/belike -V' | head -1)" "belike $(sed -n 's/^VERSION *= *//p' Makefile)"
out=$(maulctl nb '/tmp/belike -1; echo "status=$?"')
case "$out" in *"(open-meteo 20"*"status=0") echo "ok   live forecast: $(echo "$out" | head -n 1)";; *) echo "FAIL live forecast: $out"; fail=1;; esac
check "no anchors -> exit 76" "$(maulctl nb 'WEATHER_NO_TA=1 /tmp/belike-test-hooks -1 >/dev/null 2>&1; echo $?')" 76
check "TLS follows the platform clock (year 2100 -> exit 76)" "$(maulctl nb 'WEATHER_NOW=4102444800 /tmp/belike-test-hooks -1 >/dev/null 2>&1; echo $?')" 76
check "clock before 2025 -> exit 71" "$(maulctl nb 'WEATHER_NOW=100 /tmp/belike-test-hooks -1 >/dev/null 2>&1; echo $?')" 71
check "-S saves and keeps comments" "$(maulctl nb 'printf "# keep\nattempts = 1\n" > /tmp/wx-rc; /tmp/belike -c /tmp/wx-rc -l 43.07,-89.38 -n Madison -S; cat /tmp/wx-rc; rm -f /tmp/wx-rc' | tr '\n' '|')" "# keep|attempts = 1|latitude = 43.0700|longitude = -89.3800|name = Madison|units = imperial|"
check "bad host -> exit 68" "$(maulctl nb '/tmp/belike -q -t 3 -H no-such-host.invalid:80 >/dev/null 2>&1; echo $?')" 68
check "bad option -> exit 64" "$(maulctl nb '/tmp/belike -Z >/dev/null 2>&1; echo $?')" 64
[ $fail = 0 ] && echo "maul smoke test passed" || { echo "maul smoke test FAILED"; exit 1; }
