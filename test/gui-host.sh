#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# GUI test of xbelike on this machine, in a private Xvfb. Screenshots go to build/gui-host/.
# Needs: Xvfb, ImageMagick (import, convert), python3. Skips (exit 0) when Xvfb is missing.
cd "$(dirname "$0")/.."
IM=convert; command -v magick >/dev/null 2>&1 && IM=magick
command -v Xvfb >/dev/null 2>&1 || { echo "gui-host: Xvfb not installed, skipping"; exit 0; }
. test/gui-lib.sh
OUT=build/gui-host; rm -rf $OUT; mkdir -p $OUT/run
BIN_DIR=${WX_BUILD:-build/host}
cp "$BIN_DIR/xbelike" "$BIN_DIR/xbelike-search-test" "$BIN_DIR/xbelike-stale-test" "$BIN_DIR/xdrive" test/gui/scenario.sh $OUT/run/
python3 test/cache_fixture.py $OUT/run/rcCache.cache
python3 test/cache_fixture.py $OUT/run/rcStale.cache 10800
XPID=
cleanup() {
    fixture_cleanup
    if [ -n "$XPID" ]; then kill "$XPID" 2>/dev/null || :; wait "$XPID" 2>/dev/null || :; fi
}
trap 'cleanup' EXIT
trap 'exit 1' INT TERM
start_fixtures || exit 1
Xvfb -displayfd 3 -screen 0 640x420x24 -nolisten tcp -noreset > "$OUT/xvfb.log" 2>&1 3>"$OUT/display.ready" &
XPID=$!
number=$(python3 test/wait-ready.py "$OUT/display.ready" "$XPID") || { cat "$OUT/xvfb.log"; exit 1; }
DISP=:$number
( cd $OUT/run && WX_DIR=. DISP=$DISP XVFB_EXTERNAL=1 SNAPEXT=png XDRIVE_SNAPCMD="import -display $DISP -window root" \
    PORT_OK=$PORT_OK PORT_SLOW=$PORT_SLOW PORT_500=$PORT_500 GALLERY="$GALLERY" sh ./scenario.sh ) 2>&1 | tee $OUT/log.txt
for f in $OUT/run/snap/*.png; do
	[ -f "$f" ] && $IM "$f" -crop 440x300+120+90 +repage "$OUT/$(basename "$f")"
done
grep -q 'gui scenario passed' $OUT/log.txt
