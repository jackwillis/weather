#!/bin/sh
# Feature tests: run the real binary against a misbehaving local server and
# check what a user or script would see (output text, exit status).
# usage: sh test/feature.sh path/to/weather        ONLINE=1 also tests the real server
BIN=${1:-build/host/weather}
HERE=$(dirname "$0")
BASE=${PORT_BASE:-18700}
pass=0; fail=0
pids=""
trap 'kill $pids 2>/dev/null' EXIT INT TERM

ok()   { pass=$((pass+1)); }
bad()  { fail=$((fail+1)); echo "FAIL: $1"; }

# expect NAME EXPECTED_STATUS ARGS...   (stdout+stderr kept in $OUT)
expect() {
	name=$1; want=$2; shift 2
	OUT=$("$BIN" "$@" 2>&1); got=$?
	if [ "$got" = "$want" ]; then ok; else bad "$name: exit $got, wanted $want ($OUT)"; fi
}
contains() { case "$OUT" in *"$2"*) ok;; *) bad "$1: output lacks '$2' (got: $OUT)";; esac; }

serve() {  # serve MODE -> sets $P to the port
	P=$BASE; BASE=$((BASE+1))
	python3 "$HERE/fixture_server.py" "$1" "$P" & pids="$pids $!"
}

serve ok; P_OK=$P
serve 500; P_500=$P
serve 404; P_404=$P
serve slow; P_SLOW=$P
serve chunked; P_CHUNK=$P
serve trunc; P_TRUNC=$P
serve garbage; P_GARB=$P
serve hangup; P_HANG=$P
serve badjson; P_BADJ=$P
sleep 1

CFG=$(mktemp); echo "attempts = 1" > "$CFG"
W="-c $CFG"

expect "ok oneline" 0 $W -1 -H 127.0.0.1:$P_OK
contains "ok oneline text" "54.8F Clear sky, wind 8.7mph SE, humidity 84% (open-meteo 2026-10-24T04:45)"
expect "ok metric flag" 0 $W -u metric -1 -H 127.0.0.1:$P_OK
expect "ok full" 0 $W -H 127.0.0.1:$P_OK
contains "full layout" "Weather from open-meteo, observed 2026-10-24T04:45 UTC"
contains "full wind line" "Wind 8.7 mph from SE   Humidity 84%"
expect "plain-http warning is shown" 0 $W -1 -H 127.0.0.1:$P_OK
contains "warn" "NO TLS"
expect "quiet hides warning" 0 $W -q -1 -H 127.0.0.1:$P_OK
case "$OUT" in *"NO TLS"*) bad "quiet still warns";; *) ok;; esac
expect "debug dumps traffic" 0 $W -d -1 -H 127.0.0.1:$P_OK
contains "debug request" "GET /v1/forecast?latitude=41.8800"
contains "debug response" "HTTP/1.0 200 OK"

expect "http 500" 76 $W -H 127.0.0.1:$P_500
contains "500 message" "http: server returned an error status (detail 500)"
expect "http 404" 76 $W -H 127.0.0.1:$P_404
expect "chunked refused" 65 $W -H 127.0.0.1:$P_CHUNK
expect "truncated body" 74 $W -H 127.0.0.1:$P_TRUNC
expect "garbage reply" 65 $W -H 127.0.0.1:$P_GARB
expect "hangup" 65 $W -H 127.0.0.1:$P_HANG
expect "bad json" 65 $W -H 127.0.0.1:$P_BADJ
expect "read timeout" 75 $W -t 1 -H 127.0.0.1:$P_SLOW
expect "connection refused" 69 $W -H 127.0.0.1:1
expect "unknown host" 68 $W -H no-such-host.invalid:80

# retries: 3 attempts with backoff against a 500 should log all three
echo "attempts = 3" > "$CFG"
expect "retries on 5xx" 76 $W -H 127.0.0.1:$P_500
contains "attempt 3 logged" "attempt 3/3"
expect "no retry on 404" 76 $W -H 127.0.0.1:$P_404
case "$OUT" in *"attempt 2/3"*) bad "404 was retried";; *) ok;; esac

# usage and config errors
expect "version" 0 -V
contains "version text" "weather 0."
expect "help" 0 -h
expect "bad option" 64 -Z
expect "stray argument" 64 foo
expect "bad -l" 64 -l banana
expect "-l out of range" 64 -l 95,10
expect "bad units" 64 -u kelvin
expect "unknown source" 64 -p nope -H 127.0.0.1:$P_OK
printf 'units = metric\nbogus = 1\n' > "$CFG"
expect "bad config" 78 -c "$CFG"
contains "config line number" "line 2"
expect "missing explicit config" 78 -c /nonexistent/weatherrc

# stdout must carry only the result; diagnostics go to stderr
echo "attempts = 1" > "$CFG"
SOUT=$("$BIN" -c "$CFG" -1 -H 127.0.0.1:$P_OK 2>/dev/null)
case "$SOUT" in "54.8F Clear sky"*) ok;; *) bad "stdout polluted: $SOUT";; esac
case "$SOUT" in *weather:*) bad "log text on stdout";; *) ok;; esac

if [ -n "$ONLINE" ]; then
	expect "online live fetch" 0 -1
	WEATHER_NO_TA=1 "$BIN" -1 >/dev/null 2>"$CFG.err"; got=$?
	if [ "$got" = 76 ] && grep -q "cert" "$CFG.err"; then ok; else bad "no-anchor run: exit $got $(cat $CFG.err)"; fi
	rm -f "$CFG.err"
fi

rm -f "$CFG"
echo "feature tests: $pass passed, $fail failed"
[ "$fail" = 0 ]
