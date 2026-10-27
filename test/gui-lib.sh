# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Shared by gui-host.sh and gui-maul.sh: fixture servers for the GUI scenario.
GALLERY_MODES="night-clear:w:0:0:3.4 day-clear:w:0:1:21.0 partly-day:w:2:1:18.5 partly-night:w:2:0:12.0 overcast:w:3:1:9.0 fog:w:45:1:4.0 drizzle:w:51:1:7.5 rain:w:63:1:11.0 heavy-rain:w:65:0:10.0 snow:w:73:1:-3.0 storm:w:95:0:22.0"
start_fixtures() { # sets PORT_OK PORT_SLOW PORT_500 GALLERY PORTS and SERVER_PIDS
	BASE=${1:-18800}
	PORT_OK=$BASE; PORT_SLOW=$((BASE+1)); PORT_500=$((BASE+2)); PORTS="$PORT_OK $PORT_SLOW $PORT_500"
	python3 test/fixture_server.py ok $PORT_OK & SERVER_PIDS=$!
	python3 test/fixture_server.py slow $PORT_SLOW & SERVER_PIDS="$SERVER_PIDS $!"
	python3 test/fixture_server.py 500 $PORT_500 & SERVER_PIDS="$SERVER_PIDS $!"
	GALLERY=""; n=3
	for m in $GALLERY_MODES; do
		name=${m%%:*}; mode=${m#*:}; port=$((BASE+n)); n=$((n+1))
		python3 test/fixture_server.py "$mode" $port & SERVER_PIDS="$SERVER_PIDS $!"
		GALLERY="$GALLERY $name:$port"; PORTS="$PORTS $port"
	done
	sleep 1
}
