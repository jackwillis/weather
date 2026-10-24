#!/bin/sh
# GUI test of the real PowerPC xweather on maul (NetBSD), inside Xvfb there, with the same scenario as
# gui-host.sh. Screenshots (xwd, converted here) go to build/gui/. Needs maul up in NetBSD, python3, ImageMagick.
cd "$(dirname "$0")/.."
IM=convert; command -v magick >/dev/null 2>&1 && IM=magick
. test/gui-lib.sh
OUT=build/gui; rm -rf $OUT; mkdir -p $OUT
trap 'kill $SERVER_PIDS 2>/dev/null' EXIT INT TERM
start_fixtures 18800
maulctl nb 'rm -rf /tmp/wx; mkdir -p /tmp/wx' || { echo "cannot reach maul"; exit 1; }
for f in build/netbsd-macppc/xweather build/netbsd-macppc/xdrive; do maulctl push nb $f wx/$(basename $f) >/dev/null || exit 1; done
maulctl push nb test/gui/scenario.sh wx/scenario.sh >/dev/null || exit 1
maulctl nb 'chmod 755 /tmp/wx/xweather /tmp/wx/xdrive'
FWD=""; for p in $PORTS; do FWD="$FWD -R $p:127.0.0.1:$p"; done
NB_SSH_OPTS="$FWD" maulctl nb "cd /tmp/wx && WX_DIR=/tmp/wx DISP=:77 SNAPEXT=xwd PORT_OK=$PORT_OK PORT_SLOW=$PORT_SLOW PORT_500=$PORT_500 GALLERY='$GALLERY' sh /tmp/wx/scenario.sh" 2>&1 | tee $OUT/log.txt
for f in $(maulctl nb 'ls /tmp/wx/snap' 2>/dev/null); do
	maulctl nb "cat /tmp/wx/snap/$f" > $OUT/$f
	$IM $OUT/$f -crop 440x300+120+90 +repage $OUT/${f%.xwd}.png 2>/dev/null
done
grep -q 'gui scenario passed' $OUT/log.txt
