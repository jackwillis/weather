#!/bin/sh
# Tests for tools/check-anchors.sh: it must accept the real tree and reject a tampered one.
# Run from the repo root, as make does.   usage: sh test/anchors.sh path/to/brssl
BRSSL=${1:?usage: anchors.sh path/to/brssl}
case $BRSSL in /*) ;; *) BRSSL=$PWD/$BRSSL;; esac
pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "FAIL: $1"; }

T=$(mktemp -d) || exit 1
trap 'rm -rf "$T"' EXIT INT TERM
W=$T/tree

# A scratch copy of just what check-anchors reads, so tests can tamper with it.
fresh() {
	rm -rf "$W"; mkdir -p "$W/src/tls"
	cp -R tools "$W/tools"
	cp src/tls/roots.pem src/tls/anchors.h "$W/src/tls/"
}
regen() { sh "$W/tools/gen-anchors.sh" "$BRSSL" > "$W/src/tls/anchors.h.new" 2>/dev/null && mv "$W/src/tls/anchors.h.new" "$W/src/tls/anchors.h"; }
run() { OUT=$(sh "$W/tools/check-anchors.sh" "$BRSSL" 2>&1); got=$?; }
contains() { case "$OUT" in *"$2"*) ok;; *) bad "$1: output lacks '$2' (got: $OUT)";; esac; }

fresh; run
if [ "$got" = 0 ]; then ok; else bad "real tree: exit $got ($OUT)"; fi
contains "real tree" "PEM fingerprints are the expected"

# anchors.h edited by hand: must be reported as out of date.
fresh; echo "// hand edit" >> "$W/src/tls/anchors.h"; run
if [ "$got" != 0 ]; then ok; else bad "hand-edited anchors.h accepted"; fi
contains "hand-edited anchors.h" "out of date"

# A root removed from the PEM, anchors.h regenerated to match: only the fingerprint check can catch it.
fresh; sed '/END CERTIFICATE/q' src/tls/roots.pem > "$W/src/tls/roots.pem"; regen; run
if [ "$got" != 0 ]; then ok; else bad "PEM with missing roots accepted"; fi
contains "missing roots" "unexpected or missing"

# An extra root (here a duplicate of the whole set).
fresh; cat src/tls/roots.pem src/tls/roots.pem > "$W/src/tls/roots.pem"; regen; run
if [ "$got" != 0 ]; then ok; else bad "PEM with extra roots accepted"; fi
contains "extra roots" "unexpected or missing"

echo "anchors tests: $pass passed, $fail failed"
[ "$fail" = 0 ]
