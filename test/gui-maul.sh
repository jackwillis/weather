#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# GUI test of the real PowerPC xweather on maul (NetBSD), inside Xvfb there, with the same scenario as
# gui-host.sh. Screenshots (xwd, converted here) go to build/gui/. Needs maul up in NetBSD, python3, ImageMagick.
cd "$(dirname "$0")/.."
IM=convert; command -v magick >/dev/null 2>&1 && IM=magick
. test/gui-lib.sh
OUT=build/gui; rm -rf $OUT; mkdir -p $OUT
trap 'kill $SERVER_PIDS 2>/dev/null' EXIT INT TERM
start_fixtures 18800
REMOTE_DIR=$(maulctl nb 'mktemp -d /tmp/wx-gui.XXXXXXXX') || { echo "cannot reach maul"; exit 1; }
case "$REMOTE_DIR" in
/tmp/wx-gui.????????) ;;
*) echo "unexpected remote test directory"; exit 1 ;;
esac
for f in build/netbsd-macppc/xweather build/netbsd-macppc/xweather-search-test build/netbsd-macppc/xdrive; do maulctl push nb $f ${REMOTE_DIR#/tmp/}/$(basename $f) >/dev/null || exit 1; done
maulctl push nb test/gui/scenario.sh ${REMOTE_DIR#/tmp/}/scenario.sh >/dev/null || exit 1
maulctl nb "chmod 755 $REMOTE_DIR/xweather $REMOTE_DIR/xweather-search-test $REMOTE_DIR/xdrive"
FWD=""; for p in $PORTS; do FWD="$FWD -R $p:127.0.0.1:$p"; done
NB_SSH_OPTS="$FWD" maulctl nb "cd $REMOTE_DIR && WX_DIR=$REMOTE_DIR DISP=:77 SNAPEXT=xwd PORT_OK=$PORT_OK PORT_SLOW=$PORT_SLOW PORT_500=$PORT_500 GALLERY='$GALLERY' sh $REMOTE_DIR/scenario.sh" 2>&1 | tee $OUT/log.txt
for f in $(maulctl nb "ls $REMOTE_DIR/snap" 2>/dev/null); do
	maulctl nb "cat $REMOTE_DIR/snap/$f" > $OUT/$f
	$IM $OUT/$f -crop 440x300+120+90 +repage $OUT/${f%.xwd}.png 2>/dev/null
done
if grep -q 'gui scenario passed' $OUT/log.txt; then
    maulctl nb "rm -rf $REMOTE_DIR"
else
    echo "gui-maul: failure evidence retained in $REMOTE_DIR"
    exit 1
fi
