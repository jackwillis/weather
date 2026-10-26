#!/bin/sh
# Serialized Xlib widget regression; do not run beside another GUI suite on :88.
set -eu
command -v Xvfb >/dev/null 2>&1 || { echo 'field: Xvfb missing, skipping'; exit 0; }
state=$(mktemp -d /tmp/weather-field.XXXXXXXX)
server=
cleanup() {
    if [ -n "$server" ]; then kill "$server" 2>/dev/null || :; wait "$server" 2>/dev/null || :; fi
    rm -rf "$state"
}
trap cleanup 0
trap 'exit 1' HUP INT TERM
Xvfb :88 -screen 0 640x480x24 -nolisten tcp -noreset >"$state/xvfb.log" 2>&1 &
server=$!
DISPLAY=:88 "$@"
kill -0 "$server" || { cat "$state/xvfb.log"; exit 1; }
