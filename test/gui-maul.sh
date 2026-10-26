#!/bin/sh
# GUI test of the real PowerPC xweather on maul (NetBSD), inside Xvfb there, with the same scenario as
# gui-host.sh. Screenshots (xwd, converted here) go to build/gui/. Needs maul up in NetBSD, python3, ImageMagick.
cd "$(dirname "$0")/.."
IM=convert; command -v magick >/dev/null 2>&1 && IM=magick
. test/gui-lib.sh
OUT=build/gui; rm -rf $OUT; mkdir -p $OUT
trap 'fixture_cleanup' EXIT
trap 'exit 1' INT TERM
start_fixtures || exit 1
REMOTE=$(maulctl nb 'mktemp -d /tmp/wx-gui.XXXXXXXX') || { echo "cannot reach maul"; exit 1; }
case "$REMOTE" in /tmp/wx-gui.*) ;; *) echo "unexpected remote temporary path"; exit 1;; esac
NAME=${REMOTE#/tmp/}
suffix=${NAME#wx-gui.}
case "$suffix" in ''|*[!A-Za-z0-9]*) echo "invalid remote temporary suffix"; exit 1;; esac
[ "${#suffix}" -eq 8 ] || { echo "invalid remote temporary length"; exit 1; }
for f in build/netbsd-macppc/xweather build/netbsd-macppc/xdrive; do maulctl push nb $f "$NAME/$(basename "$f")" >/dev/null || exit 1; done
maulctl push nb test/gui/scenario.sh "$NAME/scenario.sh" >/dev/null || exit 1
maulctl nb "chmod 755 $REMOTE/xweather $REMOTE/xdrive" || exit 1
FWD="-o ExitOnForwardFailure=yes"; for p in $PORTS; do FWD="$FWD -R $p:127.0.0.1:$p"; done
NB_SSH_OPTS="$FWD" maulctl nb "cd $REMOTE && WX_DIR=$REMOTE DISP=:77 SNAPEXT=xwd PORT_OK=$PORT_OK PORT_SLOW=$PORT_SLOW PORT_500=$PORT_500 GALLERY='$GALLERY' sh $REMOTE/scenario.sh" 2>&1 | tee $OUT/log.txt
for f in $(maulctl nb "ls $REMOTE/snap" 2>/dev/null); do
	maulctl nb "cat $REMOTE/snap/$f" > $OUT/$f
	$IM $OUT/$f -crop 440x300+120+90 +repage $OUT/${f%.xwd}.png 2>/dev/null
done
if grep -q 'gui scenario passed' $OUT/log.txt; then
    maulctl nb "rm -rf $REMOTE"
else
    echo "gui-maul: retained failure evidence in $REMOTE"; exit 1
fi
