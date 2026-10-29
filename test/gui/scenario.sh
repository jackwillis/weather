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
XVFB_ARGS=${XVFB_ARGS:--noreset}
XVFB=${XVFB:-Xvfb}; EXT=${SNAPEXT:-xwd}
rm -rf snap; mkdir snap
fail=0; pass=0
check() { if [ "$2" = "$3" ]; then pass=$((pass+1)); echo "ok   $1"; else fail=$((fail+1)); echo "FAIL $1: got '$2' wanted '$3'"; fi; }
snap() { ./xdrive snap snap/$1.$EXT; }
fetches() { c=$(grep -c 'trying open-meteo' xw.log); echo ${c:-0}; }
start_xw() { # start_xw RC PORT [extra args]   (-v logs one line per fetch)
	rc=$1; port=$2; shift 2
	${XW_BIN:-./xweather} -v -c "$rc" -H 127.0.0.1:$port "$@" > xw.log 2>&1 &
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
	./xdrive key Escape sleep 300 click 138 13 sleep 300 click 150 40 sleep 300 snap snap/A10-about.$EXT click 110 250 sleep 300 snap snap/A10b-credits-1.$EXT key space sleep 300 snap snap/A10c-credits-2.$EXT key space sleep 300 snap snap/A10d-credits-3.$EXT click 110 250 sleep 300 snap snap/A10e-about-again.$EXT click 290 250 sleep 300 snap snap/A11-closed.$EXT
	keeplog A
	quit_xw A
fi

# ---------------------------------------------------------------- B: closing the window during a stuck fetch
printf 'attempts = 1\ntimeout = 30\n' > rcB
if start_xw rcB $PORT_SLOW; then
	./xdrive sleep 1500 snap snap/B01-fetching.$EXT
	t0=$(date +%s)
	./xdrive close; ./xdrive gone Weather 6000; check "B: window closes while the fetch is stuck" "$?" 0
	wait $XW; check "B: exit status" "$?" 0
	t1=$(date +%s)
	check "B: ...promptly, not after the 30 s timeout" "$([ $((t1 - t0)) -lt 8 ] && echo yes || echo no)" yes
fi

# ---------------------------------------------------------------- C: server errors
printf 'attempts = 1\nproviders = open-meteo\n' > rcC
if start_xw rcC $PORT_500; then
	./xdrive sleep 2500 snap snap/C01-failed.$EXT
	check "C: failure is tried once" "$(fetches)" 1
	check "C: failure is logged" "$(grep -c 'http: server returned an error status' xw.log)" 2
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
	./xdrive key Escape up 160 243 key l ctrl sleep 200 key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace type 11 down 160 243 key Return sleep 1500 up 160 243 sleep 500
	check "E: Return while holding OK applies exactly once" "$(fetches)" 2
	./xdrive key l ctrl sleep 200 key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace type 12 down 160 243 up 10 10 sleep 300
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
	check "F: a name-only edit saves without refetching" "$(fetches)" 1
	keeplog F
	quit_xw F
fi

# Saving a location must succeed before the old configuration/data are replaced.
printf 'attempts = 1\nautorefresh = off\n' > rcSave
if start_xw rcSave $PORT_OK; then
    ./xdrive sleep 1500
    mv rcSave rcSave.before; mkdir rcSave
    ./xdrive key l ctrl sleep 200
    ./xdrive key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace type 12 key Return sleep 1200
    check "Save: failure preserves current forecast without refetch" "$(fetches)" 1
    snap Save01-failed
    rmdir rcSave; mv rcSave.before rcSave
    ./xdrive key Return sleep 1200
    check "Save: retry in the open dialog saves coordinates" "$(grep -c 'latitude = 12.0000' rcSave)" 1
    check "Save: successful retry refetches once" "$(fetches)" 2
    quit_xw Save
fi

# Fixture lookup is a separately linked executable, never a production endpoint override.
if [ -x ./xweather-search-test ]; then
    XW_BIN=./xweather-search-test
    printf 'attempts = 1\nautorefresh = off\n' > rcSearch
    if start_xw rcSearch $PORT_OK; then
        ./xdrive sleep 1500 key l ctrl sleep 200 key f ctrl sleep 200
        check "Search: opening the search sends no query" "$(grep -c 'X11 fixture place lookup' xw.log)" 0
        ./xdrive key u ctrl type Madison key Return sleep 400
        snap Search01-results
        check "Search: explicit action performs one lookup" "$(grep -c 'X11 fixture place lookup' xw.log)" 1
        ./xdrive click 170 267 sleep 200
        check "Search: Use without a choice does not refetch" "$(fetches)" 1
        ./xdrive key Down key Down key Return sleep 300
        snap Search02-selected-fields
        check "Search: choosing only fills fields without saving" "$(grep -c latitude rcSearch)" 0
        check "Search: choosing does not refetch" "$(fetches)" 1
        ./xdrive key Return sleep 1500
        check "Search: Apply saves chosen coordinates" "$(grep -c 'latitude = 45.5000' rcSearch)" 1
        check "Search: Apply saves chosen display name" "$(grep -c 'name = Montr?al' rcSearch)" 1
        check "Search: selection preserves units" "$(grep -c 'units = imperial' rcSearch)" 1
        check "Search: Apply refetches once" "$(fetches)" 2
        ./xdrive key l ctrl sleep 150 key f ctrl sleep 150 key u ctrl type Fail key Return sleep 300
        snap Search03-failed
        ./xdrive key Escape key Escape sleep 200
        check "Search: failure/cancel preserve config" "$(grep -c 'latitude = 45.5000' rcSearch)" 1
        check "Search: failure/cancel keep last forecast" "$(fetches)" 2
        ./xdrive key l ctrl sleep 150 key f ctrl sleep 150 key u ctrl type Empty key Return sleep 300
        snap Search04-empty
        ./xdrive key Escape key Escape sleep 200
        ./xdrive key l ctrl sleep 150 key f ctrl sleep 150 key u ctrl type Slow key Return sleep 300 key Escape sleep 400
        check "Search: Escape cancels the blocking lookup" "$(grep -c 'X11 fixture search cancelled' xw.log)" 1
        snap Search05-cancelled-to-location
        check "Search: cancelled lookup does not refetch" "$(fetches)" 2
        ./xdrive key Escape
        quit_xw Search
        if start_xw rcSearch $PORT_OK; then
            ./xdrive sleep 1200 key l ctrl sleep 200
            snap Search06-restart
            ./xdrive key Return sleep 300
            check "Search: restart keeps chosen location without another change" "$(fetches)" 1
            ./xdrive click 332 189 sleep 150 key u ctrl type Slow key Return sleep 300
            quit_xw Search-restart
        fi
    fi
    unset XW_BIN
fi

# Empty settings show the default location and fetch it; nothing is written until a location is saved (#198).
: > rcFirst
if start_xw rcFirst $PORT_OK; then
    ./xdrive sleep 1500
    check "First: default location is fetched without a dialog" "$(fetches)" 1
    check "First: nothing is written by launching" "$(wc -c < rcFirst | tr -d ' ')" 0
    snap First01-default
    ./xdrive key l ctrl sleep 300
    snap First02-location-opened
    ./xdrive key Escape sleep 300
    check "First: Escape in Location closes only the dialog" "$(kill -0 "$XW" 2>/dev/null; echo $?)" 0
    check "First: cancelling Location leaves the file empty" "$(wc -c < rcFirst | tr -d ' ')" 0
    ./xdrive key l ctrl sleep 200 key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace key BackSpace type 12 key Return sleep 1500
    check "First: an explicit save writes the chosen location" "$(grep -c 'latitude = 12.0000' rcFirst)" 1
    check "First: the explicit save refetches once" "$(fetches)" 2
    quit_xw First
fi
# An invalid settings file is reported and never replaced.
printf 'latitude = banana\n' > rcBad
cp rcBad rcBad.before
./xweather -c rcBad > xw-bad.log 2>&1; bad=$?
check "First: invalid settings exit with the config error" "$bad" 78
check "First: invalid settings are not replaced" "$(cmp -s rcBad rcBad.before; echo $?)" 0

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
