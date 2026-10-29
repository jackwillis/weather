#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Inspect the actual linked artifacts; production must not even name the hooks.
set -eu
production=${1:?usage: test-builds.sh PRODUCTION TEST_HOOKS}
testing=${2:?usage: test-builds.sh PRODUCTION TEST_HOOKS}
work=$(mktemp -d /tmp/belike-build-modes.XXXXXXXX)
trap 'rm -rf "$work"' 0
trap 'exit 1' INT TERM
strings "$production" >"$work/production"
strings "$testing" >"$work/testing"
for name in WEATHER_NOW WEATHER_NO_TA; do
    if grep -qx "$name" "$work/production"; then echo "FAIL: production includes $name"; exit 1; fi
    grep -qx "$name" "$work/testing" || { echo "FAIL: test build lacks $name"; exit 1; }
done
echo 'test build isolation: production omits both hooks; test binary retains both'
