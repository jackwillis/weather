#!/bin/sh
# GUI test of the real PowerPC xweather on maul, inside Xvfb, with screenshots in build/gui/.
# Needs: maul booted into NetBSD, python3, ImageMagick (convert) on this machine.
set -u
cd "$(dirname "$0")/.."
OUT=build/gui; rm -rf $OUT; mkdir -p $OUT
PORT=18801
python3 test/fixture_server.py ok $PORT & SRV=$!
trap 'kill $SRV 2>/dev/null' EXIT INT TERM
sleep 1
maulctl nb 'rm -rf /tmp/wx; mkdir -p /tmp/wx' || { echo "cannot reach maul"; exit 1; }
for f in build/netbsd-macppc/xweather build/netbsd-macppc/xdrive; do maulctl push nb $f wx/$(basename $f) >/dev/null || exit 1; done
maulctl push nb test/gui/maul-scenario.sh wx/scenario.sh >/dev/null || exit 1
maulctl nb 'chmod 755 /tmp/wx/xweather /tmp/wx/xdrive'
NB_SSH_OPTS="-R $PORT:127.0.0.1:$PORT" maulctl nb "sh /tmp/wx/scenario.sh $PORT" 2>&1 | tee $OUT/log.txt
status=$?
for f in $(maulctl nb 'ls /tmp/wx/snap' 2>/dev/null); do
	maulctl nb "cat /tmp/wx/snap/$f" > $OUT/$f
	convert $OUT/$f -crop 440x300+120+90 +repage $OUT/${f%.xwd}.png 2>/dev/null
	convert $OUT/$f $OUT/${f%.xwd}-full.png 2>/dev/null
done
ls $OUT/*.png 2>/dev/null | head -40
grep -q 'gui scenario passed' $OUT/log.txt
