#!/bin/sh
# Push the cross-built binary to maul (NetBSD) and check it for real.
# Needs maulctl (see the maul notes) and an up NetBSD session on maul.
BIN=${1:-build/macppc/weather}
fail=0
check() { if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: got '$2' wanted '$3'"; fail=1; fi; }

maulctl push nb "$BIN" weather >/dev/null || { echo "cannot push to maul"; exit 1; }
maulctl nb 'chmod 755 /tmp/weather'
check "file type" "$(file "$BIN" | sed 's/.*ELF 32-bit MSB.*PowerPC.*NetBSD.*/macppc-ok/;t;s/.*/WRONG/')" macppc-ok
check "no libstdc++" "$(readelf -d "$BIN" 2>/dev/null | grep -c libstdc || true)" 0
check "version" "$(maulctl nb '/tmp/weather -V')" "weather $(sed -n 's/^VERSION = //p' Makefile)"
out=$(maulctl nb '/tmp/weather -1; echo "status=$?"')
case "$out" in *"(open-meteo 20"*"status=0") echo "ok   live forecast: $(echo "$out" | head -n 1)";; *) echo "FAIL live forecast: $out"; fail=1;; esac
check "no anchors -> exit 76" "$(maulctl nb 'WEATHER_NO_TA=1 /tmp/weather -1 >/dev/null 2>&1; echo $?')" 76
check "bad host -> exit 68" "$(maulctl nb '/tmp/weather -q -t 3 -H no-such-host.invalid:80 >/dev/null 2>&1; echo $?')" 68
check "bad option -> exit 64" "$(maulctl nb '/tmp/weather -Z >/dev/null 2>&1; echo $?')" 64
[ $fail = 0 ] && echo "maul smoke test passed" || { echo "maul smoke test FAILED"; exit 1; }
