# Shared by gui-host.sh and gui-maul.sh: fixture servers for the GUI scenario.
GALLERY_MODES="night-clear:w:0:0:3.4 day-clear:w:0:1:21.0 partly-day:w:2:1:18.5 partly-night:w:2:0:12.0 overcast:w:3:1:9.0 fog:w:45:1:4.0 drizzle:w:51:1:7.5 rain:w:63:1:11.0 heavy-rain:w:65:0:10.0 snow:w:73:1:-3.0 storm:w:95:0:22.0"
FIXTURE_HERE=test
. test/fixture-lib.sh
start_fixtures() { # sets PORT_OK PORT_SLOW PORT_500 GALLERY PORTS and owned children
    fixture_init || return 1
    fixture_start ok || return 1; PORT_OK=$FIXTURE_PORT
    fixture_start slow || return 1; PORT_SLOW=$FIXTURE_PORT
    fixture_start 500 || return 1; PORT_500=$FIXTURE_PORT
    PORTS="$PORT_OK $PORT_SLOW $PORT_500"; GALLERY=""
    for m in $GALLERY_MODES; do
        name=${m%%:*}; mode=${m#*:}
        fixture_start "$mode" || return 1
        GALLERY="$GALLERY $name:$FIXTURE_PORT"; PORTS="$PORTS $FIXTURE_PORT"
    done
}
