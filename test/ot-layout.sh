#!/bin/sh
# Run inside Retro68: compile actual glue, then prove its layout guards reject mutations.
# Only our source is copied. No SDK content is generated or retained.
set -eu
compiler=${1:?usage: ot-layout.sh RETRO68_CXX}
work=$(mktemp -d /tmp/weather-ot-layout.XXXXXXXX)
trap 'rm -rf "$work"' 0
trap 'exit 1' INT TERM
compile() {
    "$compiler" -std=c++17 -O2 -fno-exceptions -fno-rtti -Isrc/platform/os9 -c "$1" -o "$work/probe.o" >"$work/compiler.log" 2>&1
}
compile src/platform/os9/ot_glue.cc || { cat "$work/compiler.log"; exit 1; }
sed 's/UInt8 pad\[7\]/UInt8 pad[8]/' src/platform/os9/ot_glue.cc >"$work/size.cc"
if compile "$work/size.cc"; then echo 'FAIL: changed record size accepted'; exit 1; fi
grep -q 'OT client record layout changed' "$work/compiler.log" || { cat "$work/compiler.log"; exit 1; }
sed 's/UInt8 flag;/UInt8 extra; UInt8 flag;/' src/platform/os9/ot_glue.cc >"$work/offset.cc"
if compile "$work/offset.cc"; then echo 'FAIL: changed flag offset accepted'; exit 1; fi
grep -q 'OT client record flag offset changed' "$work/compiler.log" || { cat "$work/compiler.log"; exit 1; }
echo 'OT layout: production compiled, size and flag-offset mutations rejected'
