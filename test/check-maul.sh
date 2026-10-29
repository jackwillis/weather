#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Run the cross-built unit tests on maul (big-endian PowerPC, NetBSD): the ground truth for endianness.
BIN=${1:-build/netbsd-macppc/unittest}
ROB=${2:-build/netbsd-macppc/robust}
CONNECT=${3:-}
check_connect=0
maulctl nb 'rm -rf /tmp/wx; mkdir -p /tmp/wx/test/fixtures' || { echo "cannot reach maul"; exit 1; }
maulctl push nb "$BIN" wx/unittest >/dev/null || exit 1
maulctl push nb "$ROB" wx/robust >/dev/null || exit 1
if [ -n "$CONNECT" ]; then
    maulctl push nb "$CONNECT" wx/connect-budget-test >/dev/null || exit 1
    check_connect=1
fi
for f in test/fixtures/*; do maulctl push nb "$f" "wx/$f" >/dev/null || exit 1; done
maulctl nb "chmod 755 /tmp/wx/unittest /tmp/wx/robust; cd /tmp/wx && ./unittest && ./robust 2000 && { [ $check_connect = 0 ] || { chmod 755 ./connect-budget-test && ./connect-budget-test; }; }; echo status=\$?" | tee /tmp/belike-check-maul.out
grep -q '^status=0$' /tmp/belike-check-maul.out
