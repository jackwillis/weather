#!/bin/sh
# Tests for tools/check-anchors.sh: it must accept the real tree and reject a tampered one.
# Run from the repo root, as make does.   usage: sh test/anchors.sh path/to/brssl
BRSSL=${1:?usage: anchors.sh path/to/brssl}
case $BRSSL in /*) ;; *) BRSSL=$PWD/$BRSSL;; esac
pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "FAIL: $1"; }

T=
trap 'rm -rf "$T"' EXIT
trap 'exit 1' INT TERM
T=$(mktemp -d "${TMPDIR:-/tmp}/anchors-test.XXXXXX") || exit 1
W=$T/tree

# A scratch copy of just what check-anchors reads, so tests can tamper with it.
fresh() {
	rm -rf "$W"; mkdir -p "$W/src/tls"
	cp -R tools "$W/tools"
	cp src/tls/roots.pem src/tls/anchors.h "$W/src/tls/"
}
regen() { sh "$W/tools/gen-anchors.sh" "$BRSSL" > "$W/src/tls/anchors.h.new" 2>/dev/null && mv "$W/src/tls/anchors.h.new" "$W/src/tls/anchors.h"; }
run() { OUT=$(sh "$W/tools/check-anchors.sh" "$BRSSL" 2>&1); got=$?; }
accepts() { if [ "$got" = 0 ]; then ok; else bad "$1: exit $got ($OUT)"; fi; }
rejects() { if [ "$got" != 0 ]; then ok; else bad "$1: accepted"; fi; case "$OUT" in *"$2"*) ok;; *) bad "$1: output lacks '$2' (got: $OUT)";; esac; }
# The nth certificate block (1-based) of the real PEM.
nth_cert() { c=0; while IFS= read -r l; do case $l in *"BEGIN CERT"*) c=$((c+1));; esac; [ "$c" = "$1" ] && echo "$l"; done < src/tls/roots.pem; }

fresh; run
accepts "real tree"
case "$OUT" in *"PEM fingerprints are the expected"*) ok;; *) bad "real tree: no fingerprint confirmation (got: $OUT)";; esac

# anchors.h edited by hand: must be reported as out of date.
fresh; echo "// hand edit" >> "$W/src/tls/anchors.h"; run
rejects "hand-edited anchors.h" "out of date"

# CRLF line endings and a missing final newline are harmless and must still pass.
fresh; sed 's/$/\r/' src/tls/roots.pem > "$W/src/tls/roots.pem"; regen; run
accepts "CRLF PEM"
fresh; printf '%s' "$(cat src/tls/roots.pem)" > "$W/src/tls/roots.pem"; regen; run
accepts "PEM without final newline"

# Everything below needs the openssl fingerprint check.
if ! command -v openssl >/dev/null 2>&1; then
	echo "anchors tests: openssl not found, fingerprint cases skipped"
else
	# A root removed, anchors.h regenerated to match: only the fingerprint check can catch it.
	fresh; sed '/END CERTIFICATE/q' src/tls/roots.pem > "$W/src/tls/roots.pem"; regen; run
	rejects "missing roots" "unexpected or missing"

	# An extra root (here a duplicate of the whole set).
	fresh; cat src/tls/roots.pem src/tls/roots.pem > "$W/src/tls/roots.pem"; regen; run
	rejects "extra roots" "unexpected or missing"

	# A root swapped for a different valid certificate: same count, wrong fingerprint.
	if openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -subj /CN=impostor -days 1 \
	    -keyout "$T/k.pem" -out "$T/impostor.pem" >/dev/null 2>&1; then
		fresh; { nth_cert 1; nth_cert 2; cat "$T/impostor.pem"; } > "$W/src/tls/roots.pem"; regen; run
		rejects "substituted root" "unexpected or missing"
	else
		bad "could not generate a test certificate with openssl"
	fi

	# brssl also loads blocks labelled X509 CERTIFICATE; the check must not skip them.
	fresh; { cat src/tls/roots.pem; nth_cert 1 | sed 's/CERTIFICATE/X509 CERTIFICATE/'; } > "$W/src/tls/roots.pem"; regen; run
	rejects "X509 CERTIFICATE label" "unexpected PEM block"
fi

echo "anchors tests: $pass passed, $fail failed"
[ "$fail" = 0 ]
