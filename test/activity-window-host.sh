#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Run serially with all other display checks, using an owned private display.
set -eu
command -v Xvfb >/dev/null 2>&1 || { echo 'activity window: Xvfb missing, skipping'; exit 0; }
state=$(mktemp -d /tmp/weather-activity-window.XXXXXXXX)
server=
cleanup() {
    if [ -n "$server" ]; then kill "$server" 2>/dev/null || :; wait "$server" 2>/dev/null || :; fi
    rm -rf "$state"
}
trap cleanup 0
trap 'exit 1' HUP INT TERM
Xvfb -displayfd 3 -screen 0 640x480x24 -nolisten tcp -noreset >"$state/xvfb.log" 2>&1 3>"$state/ready" &
server=$!
number=$(python3 test/wait-ready.py "$state/ready" "$server") || { cat "$state/xvfb.log"; exit 1; }
DISPLAY=:$number "$@"
kill -0 "$server" || { cat "$state/xvfb.log"; exit 1; }
