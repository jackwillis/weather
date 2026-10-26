# Source after setting FIXTURE_HERE to this test directory. All state belongs to
# this invocation; no fixed ports, global PID files, or shared temporary paths.
fixture_init() {
    FIXTURE_STATE=$(mktemp -d "${TMPDIR:-/tmp}/weather-fixtures.XXXXXX") || return 1
    FIXTURE_PIDS=""; FIXTURE_COUNT=0
}
fixture_start() { # MODE -> FIXTURE_PORT, and append owned child to FIXTURE_PIDS
    FIXTURE_COUNT=$((FIXTURE_COUNT+1))
    ready="$FIXTURE_STATE/$FIXTURE_COUNT.ready"
    log="$FIXTURE_STATE/$FIXTURE_COUNT.log"
    python3 "$FIXTURE_HERE/fixture_server.py" "$1" 0 > "$ready" 2> "$log" &
    child=$!; FIXTURE_PIDS="$FIXTURE_PIDS $child"
    if ! FIXTURE_PORT=$(python3 "$FIXTURE_HERE/wait-ready.py" "$ready" "$child"); then
        cat "$log" >&2
        return 1
    fi
}
fixture_cleanup() {
    if [ -n "$FIXTURE_PIDS" ]; then
        kill $FIXTURE_PIDS 2>/dev/null || :
        for child in $FIXTURE_PIDS; do wait "$child" 2>/dev/null || :; done
    fi
    [ -z "$FIXTURE_STATE" ] || rm -rf "$FIXTURE_STATE"
}
