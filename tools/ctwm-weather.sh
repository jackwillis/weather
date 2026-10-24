#!/bin/sh
# Add xweather to the ctwm configuration on maul: a "Weather" entry in the root menu and
# XWeather in OccupyAll (shown on every workspace, like XClock). Idempotent, with a backup.
#   sh ctwm-weather.sh          install      (as root: the file is /etc/X11/ctwm/system.ctwmrc)
#   sh ctwm-weather.sh undo     remove exactly what this script added
# env: CTWMRC=path to test on a copy.   Afterwards choose "Restart CTWM" from the root menu.
RC=${CTWMRC:-/etc/X11/ctwm/system.ctwmrc}
MARK=maul-weather
[ -f "$RC" ] || { echo "no such file: $RC"; exit 1; }

if [ "$1" = undo ]; then
	grep -q "BEGIN $MARK" "$RC" || { echo "nothing to undo: no $MARK block in $RC"; exit 0; }
	sed "/BEGIN $MARK/,/END $MARK/d" "$RC" > "$RC.new" && cat "$RC.new" > "$RC" && rm -f "$RC.new"
	echo "removed the $MARK blocks from $RC"
	exit 0
fi

if grep -q "BEGIN $MARK" "$RC"; then echo "already installed ($MARK blocks present in $RC)"; exit 0; fi
awk -v mark=$MARK '
	/^menu "NetBSD"/ { sec = "menu" }
	/^OccupyAll/     { sec = "occ" }
	{ print }
	sec == "menu" && /File manager \(xfe\)/ {
		print "    # BEGIN " mark
		print "    \" Weather (xweather)\"\t\t!\"/usr/pkg/bin/xweather &\""
		print "    # END " mark
		menu_done = 1
	}
	sec == "occ" && /"XClock"/ {
		print "   # BEGIN " mark
		print "   \"XWeather\""
		print "   # END " mark
		occ_done = 1
		sec = ""
	}
	/^}/ { sec = "" }
	END { if (!menu_done || !occ_done) exit 3 }
' "$RC" > "$RC.new"
if [ $? -ne 0 ]; then
	rm -f "$RC.new"
	echo "FAILED: could not find the expected places in $RC (the NetBSD menu's xfe line and OccupyAll's XClock line); nothing changed"
	exit 1
fi
[ -f "$RC.pre-weather" ] || cp -p "$RC" "$RC.pre-weather"     # backup (once), only now that the edit is known good
cat "$RC.new" > "$RC" && rm -f "$RC.new"
echo "added Weather to the root menu and XWeather to OccupyAll in $RC (backup: $RC.pre-weather)"
echo "choose \"Restart CTWM\" from the root menu to load it"
