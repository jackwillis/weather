#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# GUI test of xweather on this machine, in a private Xvfb. Screenshots go to build/gui-host/.
# Needs: Xvfb, ImageMagick (import, convert), python3. Skips (exit 0) when Xvfb is missing.
cd "$(dirname "$0")/.."
IM=convert; command -v magick >/dev/null 2>&1 && IM=magick
command -v Xvfb >/dev/null 2>&1 || { echo "gui-host: Xvfb not installed, skipping"; exit 0; }
. test/gui-lib.sh
OUT=build/gui-host; rm -rf $OUT; mkdir -p $OUT/run
BIN_DIR=${WX_BUILD:-build/host}
cp "$BIN_DIR/xweather" "$BIN_DIR/xweather-search-test" "$BIN_DIR/xdrive" test/gui/scenario.sh $OUT/run/
trap 'kill $SERVER_PIDS 2>/dev/null' EXIT INT TERM
start_fixtures 18800
( cd $OUT/run && WX_DIR=. DISP=:88 SNAPEXT=png XDRIVE_SNAPCMD="import -display :88 -window root" \
    PORT_OK=$PORT_OK PORT_SLOW=$PORT_SLOW PORT_500=$PORT_500 GALLERY="$GALLERY" sh ./scenario.sh ) 2>&1 | tee $OUT/log.txt
for f in $OUT/run/snap/*.png; do
	[ -f "$f" ] && $IM "$f" -crop 440x300+120+90 +repage "$OUT/$(basename "$f")"
done
grep -q 'gui scenario passed' $OUT/log.txt
