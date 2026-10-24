#!/bin/sh
# Run the cross-built unit tests on maul (big-endian PowerPC, NetBSD): the ground truth for endianness.
BIN=${1:-build/netbsd-macppc/unittest}
ROB=${2:-build/netbsd-macppc/robust}
maulctl nb 'rm -rf /tmp/wx; mkdir -p /tmp/wx/test/fixtures' || { echo "cannot reach maul"; exit 1; }
maulctl push nb "$BIN" wx/unittest >/dev/null || exit 1
maulctl push nb "$ROB" wx/robust >/dev/null || exit 1
for f in test/fixtures/*; do maulctl push nb "$f" "wx/$f" >/dev/null || exit 1; done
maulctl nb 'chmod 755 /tmp/wx/unittest /tmp/wx/robust; cd /tmp/wx && ./unittest && ./robust 2000; echo "status=$?"' | tee /tmp/weather-check-maul.out
grep -q '^status=0$' /tmp/weather-check-maul.out
