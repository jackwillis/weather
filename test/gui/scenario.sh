#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# GUI scenario for xweather, run inside a private virtual X server. Runs on this machine
# (test/gui-host.sh) and on maul/NetBSD (test/gui-maul.sh); plain POSIX sh.
#
# env: WX_DIR (dir holding xweather and xdrive; default .)  DISP (default :77)
#      XVFB (default Xvfb)  XVFB_ARGS  SNAPEXT (xwd|png)  XDRIVE_SNAPCMD (host: import ...)
#      PORT_OK PORT_SLOW PORT_500   fixture servers on 127.0.0.1
#      GALLERY="name:port name:port ..."   one fixture server per weather picture
cd "${WX_DIR:-.}" || exit 1
export PATH=/usr/X11R7/bin:$PATH DISPLAY=${DISP:-:77}
XVFB=${XVFB:-Xvfb}; EXT=${SNAPEXT:-xwd}
rm -rf snap; mkdir snap
fail=0; pass=0
check() { if [ "$2" = "$3" ]; then pass=$((pass+1)); echo "ok   $1"; else fail=$((fail+1)); echo "FAIL $1: got '$2' wanted '$3'"; fi; }
snap() { ./xdrive snap snap/$1.$EXT; }
fetches() { c=$(grep -c 'trying open-meteo' xw.log); echo ${c:-0}; }
start_xw() { # start_xw RC PORT [extra args]   (-v logs one line per fetch)
	rc=$1; port=$2; shift 2
	./xweather -v -c "$rc" -H 127.0.0.1:$port "$@" > xw.log 2>&1 &
	XW=$!
	./xdrive wait Weather 20000 || { echo "FAIL window did not appear"; cat xw.log; fail=$((fail+1)); return 1; }
}
keeplog() { cp xw.log snap/$1.log; }
quit_xw() {
	./xdrive key q ctrl; ./xdrive gone Weather 8000; check "$1: Ctrl-Q closes the window" "$?" 0
	wait $XW; check "$1: exit status" "$?" 0
}

if [ "${XVFB_EXTERNAL:-0}" != 1 ]; then
    $XVFB $DISPLAY -screen 0 640x420x24 -nolisten tcp -noreset $XVFB_ARGS > xvfb.log 2>&1 &
    XPID=$!
    trap 'kill $XPID 2>/dev/null' EXIT
fi
i=0
until ./xdrive ping 2>/dev/null; do
	i=$((i+1)); [ $i -gt 90 ] && { echo "FAIL Xvfb did not start"; cat xvfb.log; exit 1; }
	sleep 1
done
echo "ok   virtual X server up on $DISPLAY"

# ---------------------------------------------------------------- A: normal use, good server
printf '# my weather file\nattempts = 1\n' > rcA
if start_xw rcA $PORT_OK; then
	./xdrive sleep 1000
	check "A: first fetch happens by itself" "$(fetches)" 1
	snap A01-main
	./xdrive click 64 231 key F5 key F5 key F5 sleep 500
	check "A: rapid button and F5 refreshes are ignored" "$(fetches)" 1
	./xdrive click 138 13 sleep 100 click 150 40
	XDRIVE_WINDOW='Weather Activity Log' ./xdrive wait 'Weather Activity Log' 2000
	check "A: View menu opens an independent activity window" "$?" 0
	XDRIVE_WINDOW='Weather Activity Log' ./xdrive key Home key End sleep 100 snap snap/A01b-activity.$EXT key Escape gone 'Weather Activity Log' 2000
	check "A: activity window closes independently" "$?" 0
	./xdrive wait Weather 1000
	check "A: main window survives activity close" "$?" 0
	check "A: activity browsing starts no fetch" "$(fetches)" 1
	./xdrive sleep 5000 click 64 231 sleep 1500
	check "A: Refresh button fetches" "$(fetches)" 2
	./xdrive sleep 5000 key F5 sleep 1500
	check "A: F5 fetches" "$(fetches)" 3
	./xdrive sleep 5000 click 28 13 sleep 300 snap snap/A02-file-menu.$EXT click 30 40 sleep 1500
	check "A: File > Refresh fetches" "$(fetches)" 4
	./xdrive click 80 13 sleep 300 snap snap/A03-options-menu.$EXT
	./xdrive click 28 13 sleep 300 snap snap/A04-switch-to-file.$EXT click 80 13 sleep 300      # a click on another title switches menus
	./xdrive click 100 40 sleep 600                                                    # (Options is open) > Auto-refresh (off)
	check "A: auto-refresh off is saved" "$(grep -c 'autorefresh = off' rcA)" 1
	./xdrive click 80 13 sleep 300 click 100 84 sleep 2000 snap snap/A05-metric.$EXT             # Options > Metric
	check "A: metric is saved" "$(grep -c 'units = metric' rcA)" 1
	check "A: changing units refetches" "$(fetches)" 5
	# location dialog: edit all three fields and the units box, then OK
	./xdrive click 182 231 sleep 400 snap snap/A06-location-dialog.$EXT
	./xdrive key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace type 59.91 key Tab \
	    key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace type 10.75 key Tab \
	    key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace \
	    type "Oslo, Norway" key Tab key space sleep 300 snap snap/A07-dialog-edited.$EXT
	./xdrive key Return sleep 2000 snap snap/A08-after-ok.$EXT
	check "A: dialog saves location, keeps comments and other settings" "$(tr '\n' '|' < rcA)" "# my weather file|attempts = 1|autorefresh = off|units = imperial|latitude = 59.9100|longitude = 10.7500|name = Oslo, Norway|"
	check "A: new location refetches" "$(fetches)" 6
	# a bad value is refused with an explanation; nothing is saved; Escape cancels
	./xdrive key l ctrl sleep 300 key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace type 91 key Return sleep 300 snap snap/A09-dialog-error.$EXT
	check "A: bad latitude is not saved" "$(grep -c 'latitude = 91' rcA)" 0
	check "A: dialog stayed open (no refetch)" "$(fetches)" 6
	./xdrive key Escape sleep 300 click 190 13 sleep 300 click 200 40 sleep 300 snap snap/A10-about.$EXT click 110 250 sleep 300 snap snap/A10b-credits-1.$EXT key space sleep 300 snap snap/A10c-credits-2.$EXT key space sleep 300 snap snap/A10d-credits-3.$EXT click 110 250 sleep 300 snap snap/A10e-about-again.$EXT click 290 250 sleep 300 snap snap/A11-closed.$EXT
	keeplog A
	./xdrive key j ctrl
	XDRIVE_WINDOW='Weather Activity Log' ./xdrive wait 'Weather Activity Log' 2000 key q ctrl
	./xdrive gone Weather 8000
	check "A: Ctrl-Q from activity quits the application" "$?" 0
	./xdrive gone 'Weather Activity Log' 2000
	check "A: application quit closes activity too" "$?" 0
	wait $XW; check "A: exit status" "$?" 0
fi

# ---------------------------------------------------------------- B: closing the window during a stuck fetch
printf 'attempts = 1\ntimeout = 30\n' > rcB
if start_xw rcB $PORT_SLOW; then
	./xdrive sleep 1500 snap snap/B01-fetching.$EXT
	./xdrive key j ctrl
	XDRIVE_WINDOW='Weather Activity Log' ./xdrive wait 'Weather Activity Log' 2000 key Home key End sleep 100 snap snap/B02-activity-fetching.$EXT
	check "B: activity stays responsive during a stalled fetch" "$?" 0
	XDRIVE_WINDOW='Weather Activity Log' ./xdrive close gone 'Weather Activity Log' 2000
	check "B: viewer close does not cancel the fetch" "$?" 0
	check "B: activity input starts no nested request" "$(fetches)" 1
	./xdrive wait Weather 1000
	check "B: main window remains open during fetch" "$?" 0
	./xdrive key j ctrl
	XDRIVE_WINDOW='Weather Activity Log' ./xdrive wait 'Weather Activity Log' 2000
	t0=$(date +%s)
	./xdrive close; ./xdrive gone Weather 6000; check "B: window closes while the fetch is stuck" "$?" 0
	wait $XW; check "B: exit status" "$?" 0
	./xdrive gone 'Weather Activity Log' 2000
	check "B: quitting also closes the activity window" "$?" 0
	t1=$(date +%s)
	check "B: ...promptly, not after the 30 s timeout" "$([ $((t1 - t0)) -lt 8 ] && echo yes || echo no)" yes
fi

# ---------------------------------------------------------------- C: server errors
printf 'attempts = 1\nproviders = open-meteo\n' > rcC
if start_xw rcC $PORT_500; then
	./xdrive sleep 2500 snap snap/C01-failed.$EXT
	check "C: failure is tried once" "$(fetches)" 1
	check "C: failure is logged" "$(grep -c 'http: server returned an error status' xw.log)" 2
	./xdrive key j ctrl
	XDRIVE_WINDOW='Weather Activity Log' ./xdrive wait 'Weather Activity Log' 2000 sleep 100 snap snap/C02-activity-failed.$EXT key Escape gone 'Weather Activity Log' 2000
	check "C: activity is usable after fetch failure" "$?" 0
	./xdrive sleep 5000 click 64 231 sleep 1500
	check "C: Refresh retries a failed fetch" "$(fetches)" 2
	keeplog C
	quit_xw C
fi

# ---------------------------------------------------------------- Cache: real file restoration, no service contact
printf 'providers = met-no\nattempts = 1\n' > rcCache
for round in 1 2; do
	./xweather -d -c rcCache > xw.log 2>&1 & XW=$!
	./xdrive wait Weather 20000 || { echo "FAIL cached window did not appear"; cat xw.log; cp xw.log snap/Cache-startup-failure.log; fail=$((fail+1)); break; }
	./xdrive sleep 1500 snap snap/Cache-$round.$EXT
	check "Cache $round: stale forecast restored, including restart" "$(grep -c 'restored cached forecast (stale)' xw.log)" 1
	check "Cache $round: server-valid MET reused without contact" "$(grep -c 'MET Norway: using saved forecast until server expiry' xw.log)" 1
    check "Cache $round: no MET socket is opened" "$(grep -c 'connecting to api.met.no' xw.log)" 0
    if [ "$round" = 2 ]; then
        ./xdrive click 80 13 sleep 300 snap snap/Cache-saving-menu.$EXT click 100 106 sleep 500
        check "Cache: saving off persists" "$(grep -c 'cache = off' rcCache)" 1
        check "Cache: saving off removes forecast" "$(test -e rcCache.cache && echo exists || echo absent)" absent
        ./xdrive sleep 5000 key F5 sleep 1500
        check "Cache: disabled MET is explained" "$(grep -c 'MET Norway requires forecast saving' xw.log)" 1
    fi
	keeplog Cache-$round
	quit_xw Cache-$round
done

# Saving-off survives relaunch; turning it back on is persistent too.
./xweather -d -c rcCache > xw.log 2>&1 & XW=$!
if ./xdrive wait Weather 20000; then
    ./xdrive sleep 1500 snap snap/Cache-saving-off-restart.$EXT
    check "Cache: off restart does not restore" "$(grep -c 'restored cached forecast' xw.log)" 0
    check "Cache: off restart skips MET" "$(grep -c 'MET Norway requires forecast saving' xw.log)" 1
    ./xdrive click 80 13 sleep 300 click 100 106 sleep 500
    check "Cache: re-enable persists" "$(grep -c 'cache = on' rcCache)" 1
    keeplog Cache-off-restart
    quit_xw Cache-off-restart
else
    echo "FAIL saving-off restart did not appear"; fail=$((fail+1))
fi

# ---------------------------------------------------------------- E: interrupted Location button gestures
printf 'attempts = 1\nautorefresh = off\n' > rcE
cp rcE rcE.before
if start_xw rcE $PORT_OK; then
	./xdrive sleep 1500 key l ctrl sleep 200 key Tab key Tab key End type " abandoned" down 160 243 key Escape up 160 243 sleep 1500
	check "E: Escape cancels a held OK without saving" "$(cmp -s rcE rcE.before; echo $?)" 0
	check "E: releasing cancelled OK does not refetch" "$(fetches)" 1
	./xdrive key l ctrl sleep 200 down 160 243 up 160 243 3 sleep 300
	check "E: right-button release cannot activate left-button OK" "$(fetches)" 1
	check "E: wrong-button release does not save" "$(cmp -s rcE rcE.before; echo $?)" 0
	./xdrive key Escape up 160 243 key l ctrl sleep 200 down 160 243 key Return sleep 1500 up 160 243 sleep 500
	check "E: Return while holding OK applies exactly once" "$(fetches)" 2
	./xdrive key l ctrl sleep 200 down 160 243 up 10 10 sleep 300
	check "E: release outside OK does not apply" "$(fetches)" 2
	./xdrive click 160 243 sleep 1500
	check "E: ordinary OK click still applies" "$(fetches)" 3
	keeplog E
	quit_xw E
fi

# ---------------------------------------------------------------- F: scroll and edit a full-length Location name
printf 'attempts = 1\nautorefresh = off\nname = ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu\n' > rcF
if start_xw rcF $PORT_OK; then
	./xdrive sleep 1500 key l ctrl sleep 300 key Tab key Tab key End sleep 300 snap snap/F01-name-tail.$EXT
	# The left edge now represents a scrolled character, not the start of the name.
	./xdrive click 160 152 key Delete type 0 sleep 200 snap snap/F02-name-click.$EXT
	./xdrive key Home key Delete type Z key End key BackSpace type Y sleep 200 snap snap/F03-name-edited.$EXT
	./xdrive key Return sleep 2000
	saved=$(sed -n 's/^name = //p' rcF)
	check "F: full-length name survives editing" "${#saved}" 47
	case "$saved" in Z*Y) check "F: Home and End edit visible endpoints" yes yes;; *) check "F: Home and End edit visible endpoints" no yes;; esac
	case "$saved" in *0*) check "F: click uses scrolled text position" yes yes;; *) check "F: click uses scrolled text position" no yes;; esac
	check "F: applying long name refetches once" "$(fetches)" 2
	keeplog F
	quit_xw F
fi

# ---------------------------------------------------------------- D: the weather pictures
for g in $GALLERY; do
	name=${g%%:*}; port=${g##*:}
	printf 'attempts = 1\nautorefresh = off\n' > rcD
	if start_xw rcD $port; then
		./xdrive sleep 2500 snap snap/G-$name.$EXT
		./xdrive key q ctrl; ./xdrive gone Weather 8000; wait $XW
	fi
done

echo "--- $pass checks passed, $fail failed"
[ $fail = 0 ] && echo "gui scenario passed" || echo "gui scenario FAILED"
exit $fail
