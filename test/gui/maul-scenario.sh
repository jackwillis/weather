#!/bin/sh
# Runs ON maul (NetBSD/macppc) inside a virtual X server. Started by test/gui-maul.sh, which
# pushes /tmp/wx/{xweather,xdrive} and tunnels the fixture servers to 127.0.0.1:PORT.
# usage: maul-scenario.sh PORT_OK
cd /tmp/wx || exit 1
export PATH=/usr/X11R7/bin:$PATH DISPLAY=:77
PORT=${1:-18801}
rm -rf snap rc; mkdir snap
fail=0
check() { if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: got '$2' wanted '$3'"; fail=1; fi; }

Xvfb :77 -screen 0 640x420x24 -nolisten tcp > xvfb.log 2>&1 &
XPID=$!
i=0
while ! xdpyinfo >/dev/null 2>&1; do
	i=$((i+1)); [ $i -gt 90 ] && { echo "FAIL Xvfb did not start"; cat xvfb.log; exit 1; }
	sleep 1
done
echo "ok   Xvfb up: $(xdpyinfo | grep -E 'dimensions|depth of root' | tr -s ' ' | tr '\n' ';')"

printf '# my weather file\nattempts = 1\nlog = warn\n' > rc
./xweather -c /tmp/wx/rc -H 127.0.0.1:$PORT > xw.log 2>&1 &
XW=$!
./xdrive wait Weather 20000 || { echo "FAIL window did not appear"; cat xw.log; kill $XPID; exit 1; }
echo "ok   window appeared"
./xdrive sleep 6000 snap snap/01-main.xwd
./xdrive click 20 13 sleep 400 snap snap/02-file-menu.xwd
./xdrive click 80 13 sleep 400 snap snap/03-options-menu.xwd
./xdrive key Escape click 400 150 sleep 200      # click elsewhere closes the menu
./xdrive click 180 231 sleep 400 snap snap/04-location-dialog.xwd
./xdrive key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace \
    type 59.91 key Tab key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace \
    type 10.75 key Tab \
    key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace \
    type "Oslo, Norway" key Tab key space sleep 300 snap snap/05-dialog-edited.xwd
./xdrive key Return sleep 5000 snap snap/06-after-ok.xwd
check "settings file keeps comments and gains the location" "$(cat rc | tr '\n' '|')" "# my weather file|attempts = 1|log = warn|latitude = 59.9100|longitude = 10.7500|name = Oslo, Norway|units = metric|"
# a bad value is refused and explained, nothing is saved
./xdrive click 180 231 sleep 300 key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace type 91 key Return sleep 300 snap snap/07-dialog-error.xwd
check "bad latitude is not saved" "$(grep -c 'latitude = 91' rc)" 0
./xdrive key Escape sleep 200 click 20 13 sleep 200 click 30 60 sleep 300 snap snap/08-after-refresh-menu.xwd
./xdrive click 400 13 sleep 300 snap snap/09-help-menu.xwd click 400 36 sleep 300 snap snap/10-about.xwd click 220 190 sleep 200
./xdrive key q ctrl gone Weather 10000; check "window closed on Ctrl-Q" "$?" 0
wait $XW; check "xweather exit status" "$?" 0
echo "--- xweather log"; cat xw.log
kill $XPID 2>/dev/null
[ $fail = 0 ] && echo "gui scenario passed" || echo "gui scenario FAILED"
exit $fail
