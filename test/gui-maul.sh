#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# GUI test of the real PowerPC xweather on maul (NetBSD), inside Xvfb there, with the same scenario as
# gui-host.sh. Screenshots (xwd, converted here) go to build/gui/. Needs maul up in NetBSD, python3, ImageMagick.
cd "$(dirname "$0")/.."
IM=convert; command -v magick >/dev/null 2>&1 && IM=magick
. test/gui-lib.sh
OUT=build/gui; rm -rf $OUT; mkdir -p $OUT
RUN=/tmp/wx-gui-$$
trap 'kill $SERVER_PIDS 2>/dev/null' EXIT INT TERM
start_fixtures 18800
maulctl nb "mkdir -p $RUN" || { echo "cannot reach maul"; exit 1; }
for f in build/netbsd-macppc/xweather build/netbsd-macppc/xdrive; do maulctl push nb $f ${RUN#/tmp/}/$(basename $f) >/dev/null || exit 1; done
maulctl push nb test/gui/scenario.sh ${RUN#/tmp/}/scenario.sh >/dev/null || exit 1
python3 test/cache_fixture.py $OUT/rcCache.cache
maulctl push nb $OUT/rcCache.cache ${RUN#/tmp/}/rcCache.cache >/dev/null || exit 1
maulctl nb "chmod 755 $RUN/xweather $RUN/xdrive"
FWD=""; for p in $PORTS; do FWD="$FWD -R $p:127.0.0.1:$p"; done
NB_SSH_OPTS="$FWD" maulctl nb "cd $RUN && WX_DIR=$RUN DISP=:77 SNAPEXT=xwd PORT_OK=$PORT_OK PORT_SLOW=$PORT_SLOW PORT_500=$PORT_500 GALLERY='$GALLERY' sh $RUN/scenario.sh" 2>&1 | tee $OUT/log.txt
for f in $(maulctl nb "ls $RUN/snap" 2>/dev/null); do
	maulctl nb "cat $RUN/snap/$f" > $OUT/$f
	$IM $OUT/$f -crop 440x300+120+90 +repage $OUT/${f%.xwd}.png 2>/dev/null
done
grep -q 'gui scenario passed' $OUT/log.txt
