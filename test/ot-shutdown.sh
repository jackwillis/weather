#!/bin/sh
set -eu
binary=${1:?usage: ot-shutdown.sh BINARY}
expected=$(printf 'endpoint closed\ntransport closed')
for mode in explicit exit; do
    output=$("$binary" "$mode")
    test "$output" = "$expected" || { echo "FAIL: OT shutdown $mode: $output"; exit 1; }
done
output=$("$binary" failed-init)
test -z "$output" || { echo 'FAIL: failed init closed inactive OT'; exit 1; }
echo 'OT shutdown: 3 cases passed'
