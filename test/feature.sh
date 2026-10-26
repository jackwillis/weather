#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
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
serve imperial; P_IMP=$P
serve badjson; P_BADJ=$P
serve metno; P_MET=$P
serve fallback; P_FALL=$P
sleep 1

CFG=$(mktemp); echo "attempts = 1" > "$CFG"
W="-c $CFG"

expect "ok oneline" 0 $W -1 -H 127.0.0.1:$P_OK
contains "ok oneline text" "54.8F Clear sky, wind 8.7mph SE, humidity 84% (open-meteo 2026-10-24T04:45)"
expect "ok metric flag" 0 $W -u metric -1 -H 127.0.0.1:$P_OK
contains "metric output is converted and labelled" "12.7C Clear sky, wind 14.0km/h SE"
expect "server ignoring the unit parameters is refused" 65 $W -u metric -1 -H 127.0.0.1:$P_IMP
contains "units mismatch explained" "units in the response do not match the request"
expect "ok full" 0 $W -H 127.0.0.1:$P_OK
contains "full layout" "Weather for Madison, WI from open-meteo, observed 2026-10-24T04:45 UTC"
contains "full wind line" "Wind 8.7 mph from SE   Humidity 84%"
expect "plain-http warning is shown" 0 $W -1 -H 127.0.0.1:$P_OK
contains "warn" "NO TLS"
expect "quiet hides warning" 0 $W -q -1 -H 127.0.0.1:$P_OK
case "$OUT" in *"NO TLS"*) bad "quiet still warns";; *) ok;; esac
expect "debug dumps traffic" 0 $W -d -1 -H 127.0.0.1:$P_OK
contains "debug request" "GET /v1/forecast?latitude=43.0747&longitude=-89.3841"
contains "debug response" "HTTP/1.0 200 OK"

expect "http 500" 76 $W -H 127.0.0.1:$P_500
contains "500 message" "http: server returned an error status (detail 500)"
expect "http 404" 76 $W -H 127.0.0.1:$P_404
expect "chunked refused" 65 $W -H 127.0.0.1:$P_CHUNK
expect "truncated body" 74 $W -H 127.0.0.1:$P_TRUNC
expect "garbage reply" 65 $W -H 127.0.0.1:$P_GARB
expect "hangup" 74 $W -H 127.0.0.1:$P_HANG
expect "bad json" 65 $W -H 127.0.0.1:$P_BADJ
expect "read timeout" 75 $W -t 1 -H 127.0.0.1:$P_SLOW
expect "connection refused" 69 $W -H 127.0.0.1:1
expect "unknown host" 68 $W -H no-such-host.invalid:80

# retries: 3 attempts with backoff against a 500 should log all three
echo "attempts = 3" > "$CFG"
expect "retries on 5xx" 76 $W -H 127.0.0.1:$P_500
contains "attempt 3 logged" "attempt 3/3"
expect "hangup is retried" 74 $W -H 127.0.0.1:$P_HANG
contains "hangup attempt 3 logged" "attempt 3/3"
expect "no retry on 404" 76 $W -H 127.0.0.1:$P_404
case "$OUT" in *"attempt 2/3"*) bad "404 was retried";; *) ok;; esac

# place name: default, config, -n, and -l dropping a stale name
expect "name from config" 0 $W -c "$CFG" -H 127.0.0.1:$P_OK
echo "name = Oslo" >> "$CFG"
expect "name from config file" 0 $W -H 127.0.0.1:$P_OK
contains "config name shown" "Weather for Oslo from"
expect "-l drops configured name" 0 $W -l 59.91,10.75 -H 127.0.0.1:$P_OK
contains "no name after -l" "Weather from open-meteo"
expect "-n sets name" 0 $W -l 59.91,10.75 -n Bergen -H 127.0.0.1:$P_OK
contains "-n shown" "Weather for Bergen from"
expect "-n before -l: -l still drops the name (options apply in order)" 0 $W -n Bergen -l 59.91,10.75 -H 127.0.0.1:$P_OK
contains "no name after -n then -l" "Weather from open-meteo"
expect "-l reaches the request" 0 $W -d -l 59.91,10.75 -1 -H 127.0.0.1:$P_OK
contains "-l in request" "latitude=59.9100&longitude=10.7500"
echo "attempts = 3" > "$CFG"

# second data source: MET Norway needs a contact (its terms), is skipped without one, and is the fallback
printf 'attempts = 1\ncontact = tester@example.org\n' > "$CFG.met"
expect "met-no without contact is refused" 78 -c "$CFG" -p met-no -H 127.0.0.1:$P_MET
contains "contact explained" "requires 'contact'"
expect "met-no with contact" 0 -c "$CFG.met" -1 -p met-no -H 127.0.0.1:$P_MET
contains "met-no output" "54.5F Partly cloudy, wind 7.8mph S, humidity 77% (met-no 2026-10-24T06:00)"
expect "met-no metric" 0 -c "$CFG.met" -1 -u metric -p met-no -H 127.0.0.1:$P_MET
contains "met-no metric output" "12.5C Partly cloudy, wind 12.6km/h S"
expect "contact is sent in the User-Agent" 0 -c "$CFG.met" -d -1 -p met-no -H 127.0.0.1:$P_MET
contains "user agent" "User-Agent: weather/$("$BIN" -V | sed -n "1s/^weather //p") (contact: tester@example.org)"
expect "met-no is skipped without contact, next source used" 0 -c "$CFG" -1 -p met-no,open-meteo -H 127.0.0.1:$P_OK
contains "fell back to open-meteo" "(open-meteo "
expect "falls back when the first source is broken" 0 -c "$CFG.met" -1 -p open-meteo,met-no -H 127.0.0.1:$P_FALL
contains "fell back to met-no" "(met-no "
expect "unknown source" 64 -p open-meteo,nope -H 127.0.0.1:$P_OK
rm -f "$CFG.met"

# usage and config errors
expect "version" 0 -V
contains "version text" "weather 0."
contains "version shows licence" "GPL-3.0-or-later"
expect "credits" 0 --credits
contains "credits name Open-Meteo" "Open-Meteo.com (CC BY 4.0)"
contains "credits name MET Norway" "MET Norway Locationforecast"
contains "credits name BearSSL" "BearSSL"
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

echo "attempts = 1" > "$CFG"
# Ctrl-C / SIGTERM during a fetch: clean cancel, exit 130, promptly (server never answers)
for sig in INT TERM; do
	t0=$(date +%s)
	"$BIN" $W -t 30 -H 127.0.0.1:$P_SLOW >/dev/null 2>"$CFG.sig" &
	wp=$!
	sleep 1
	kill -$sig $wp
	wait $wp; got=$?
	t1=$(date +%s)
	if [ "$got" = 130 ] && [ $((t1 - t0)) -lt 6 ]; then ok; else bad "SIG$sig during fetch: exit $got after $((t1 - t0))s"; fi
	grep -q "cancelled" "$CFG.sig" && ok || bad "SIG$sig: no 'cancelled' message ($(cat $CFG.sig))"
done
rm -f "$CFG.sig"

# -S saves location/name/units into the config file, keeping everything else
RC=$(mktemp)
printf '# my weather\nattempts = 1  # fast\nlatitude = 1.0\n' > "$RC"
expect "-S saves" 0 -c "$RC" -l 59.91,10.75 -n Bergen -u metric -S
case "$(cat "$RC")" in
	"# my weather
attempts = 1  # fast
latitude = 59.9100
longitude = 10.7500
name = Bergen
units = metric") ok;;
	*) bad "-S file contents: $(cat "$RC")";;
esac
expect "-S result is used" 0 -c "$RC" -d -1 -H 127.0.0.1:$P_OK
contains "saved latitude used" "latitude=59.9100&longitude=10.7500"
contains "saved units used" "temperature_unit=celsius"
expect "-S twice is stable" 0 -c "$RC" -l 59.91,10.75 -n Bergen -u metric -S
case "$(cat "$RC" | grep -c 'latitude')" in 1) ok;; *) bad "-S duplicated keys";; esac
if [ "$(id -u)" != 0 ]; then
	RO=$(mktemp -d)
	chmod 555 "$RO"
	expect "-S into read-only dir" 74 -c "$RO/new-rc-file" -n X -S
	chmod 755 "$RO"; rmdir "$RO"
fi
expect "-S with a bad name" 64 -c "$RC" -n 'a#b' -S
HOME= expect "-S with no HOME and no -c" 78 -S

# -c must be honoured wherever it appears: clustered (-Sc FILE), attached (-cFILE) or after other flags. The default
# rc in HOME must stay untouched, and FILE must receive the change.
HD=$(mktemp -d); OTHER=$HD/other.rc
c_case() {  # c_case NAME ARGS...
	name=$1; shift
	printf 'latitude = 1.0\n' > "$OTHER"; printf 'latitude = 2.0\n' > "$HD/.weatherrc"
	OUT=$(HOME=$HD "$BIN" "$@" 2>&1); got=$?
	if [ "$got" = 0 ]; then ok; else bad "$name: exit $got ($OUT)"; fi
	case "$(cat "$OTHER")" in *"name = Z"*) ok;; *) bad "$name: -c file not updated: $(cat "$OTHER")";; esac
	case "$(cat "$HD/.weatherrc")" in "latitude = 2.0") ok;; *) bad "$name: default rc was modified: $(cat "$HD/.weatherrc")";; esac
}
c_case "-Sc FILE" -Sc "$OTHER" -n Z
c_case "-cFILE" -c"$OTHER" -n Z -S
c_case "-c after other flags" -n Z -S -c "$OTHER"
# the last -c wins; and a flood of config options is refused, not silently truncated
printf 'latitude = 3.0\n' > "$HD/first.rc"; printf 'latitude = 4.0\n' > "$HD/second.rc"
OUT=$(HOME=$HD "$BIN" -c "$HD/first.rc" -c "$HD/second.rc" -n Z -S 2>&1); got=$?
if [ "$got" = 0 ]; then ok; else bad "two -c: exit $got ($OUT)"; fi
case "$(cat "$HD/second.rc")" in *"name = Z"*) ok;; *) bad "two -c: last file not updated";; esac
case "$(cat "$HD/first.rc")" in "latitude = 3.0") ok;; *) bad "two -c: first file was modified";; esac
many=""; i=0; while [ $i -lt 33 ]; do many="$many -n x"; i=$((i+1)); done
OUT=$(HOME=$HD "$BIN" $many 2>&1); got=$?
if [ "$got" = 64 ]; then ok; else bad "33 config options: exit $got, wanted 64 ($OUT)"; fi
case "$OUT" in *"too many options"*) ok;; *) bad "33 config options: no 'too many options' message ($OUT)";; esac
printf 'bogus = 1\n' > "$HD/bad.rc"
OUT=$("$BIN" -V -c "$HD/bad.rc" 2>&1); got=$?
if [ "$got" = 0 ]; then ok; else bad "-V with a malformed config: exit $got ($OUT)"; fi
contains "-V with a malformed config prints the version" "weather 0."
OUT=$("$BIN" -h -c "$HD/bad.rc" 2>&1); got=$?
if [ "$got" = 0 ]; then ok; else bad "-h with a malformed config: exit $got ($OUT)"; fi
rm -rf "$HD"
rm -f "$RC"

# classic Mac files end lines with a lone CR: the settings must be read, and -S must keep the file's style
CRRC=$(mktemp)
printf '# mac file\rattempts = 1\rlatitude = 59.91\rlongitude = 10.75\r' > "$CRRC"
expect "CR-only config is read" 0 -c "$CRRC" -d -1 -H 127.0.0.1:$P_OK
contains "CR-only latitude reaches the request" "latitude=59.9100&longitude=10.7500"
expect "-S on a CR-only config" 0 -c "$CRRC" -n Bergen -S
case "$(cat "$CRRC" | tr '\r' '|')" in
	"# mac file|attempts = 1|latitude = 59.9100|longitude = 10.7500|name = Bergen|units = imperial|") ok;;
	*) bad "-S on a CR-only config: $(cat "$CRRC" | tr '\r' '|')";;
esac
rm -f "$CRRC"

# stdout must carry only the result; diagnostics go to stderr
echo "attempts = 1" > "$CFG"
SOUT=$("$BIN" -c "$CFG" -1 -H 127.0.0.1:$P_OK 2>/dev/null)
case "$SOUT" in "54.8F Clear sky"*) ok;; *) bad "stdout polluted: $SOUT";; esac
case "$SOUT" in *weather:*) bad "log text on stdout";; *) ok;; esac

if [ -n "$ONLINE" ]; then
	expect "online live fetch" 0 -1
	# the TLS layer must judge certificates by the platform clock (seam), not its own idea of time
	WEATHER_NOW=4102444800 "$BIN" -1 >/dev/null 2>"$CFG.err"; got=$?
	if [ "$got" = 76 ] && grep -q "expired or not yet valid" "$CFG.err"; then ok; else bad "year-2100 clock: exit $got $(cat $CFG.err)"; fi
	WEATHER_NOW=1735689700 "$BIN" -1 >/dev/null 2>"$CFG.err"; got=$?
	if [ "$got" = 76 ] && grep -q "expired or not yet valid" "$CFG.err"; then ok; else bad "early-2025 clock: exit $got $(cat $CFG.err)"; fi
	WEATHER_NOW=100 "$BIN" -1 >/dev/null 2>"$CFG.err"; got=$?
	if [ "$got" = 71 ]; then ok; else bad "1970 clock: exit $got (wanted 71, clock)"; fi
	WEATHER_NO_TA=1 "$BIN" -1 >/dev/null 2>"$CFG.err"; got=$?
	if [ "$got" = 76 ] && grep -q "cert" "$CFG.err"; then ok; else bad "no-anchor run: exit $got $(cat $CFG.err)"; fi
	rm -f "$CFG.err"
fi

rm -f "$CFG"
echo "feature tests: $pass passed, $fail failed"
[ "$fail" = 0 ]
