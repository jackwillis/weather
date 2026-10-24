#!/bin/sh
# 1. src/tls/anchors.h must be exactly what the PEM generates (nobody hand-edited it).
# 2. The PEM must contain exactly the two roots we expect (SHA-256 fingerprints below).
# usage: tools/check-anchors.sh BRSSL
BRSSL=${1:?usage: check-anchors.sh path/to/brssl}
cd "$(dirname "$0")/.." || exit 1
tmp=$(mktemp)
sh tools/gen-anchors.sh "$BRSSL" 2>/dev/null > "$tmp"
if cmp -s "$tmp" src/tls/anchors.h; then echo "anchors: anchors.h matches isrg-roots.pem"; else echo "anchors: anchors.h is out of date (run: make anchors)"; rm -f "$tmp"; exit 1; fi
rm -f "$tmp"
want="96:BC:EC:06:26:49:76:F3:74:60:77:9A:CF:28:C5:A7:CF:E8:A3:C0:AA:E1:1A:8F:FC:EE:05:C0:BD:DF:08:C6
69:72:9B:8E:15:A8:6E:FC:17:7A:57:AF:B7:17:1D:FC:64:AD:D2:8C:2F:CA:8C:F1:50:7E:34:45:3C:CB:14:70"
if command -v openssl >/dev/null 2>&1; then
	got=$(awk '/BEGIN CERT/{f=1} f{print} /END CERT/{f=0; print "@@"}' src/tls/isrg-roots.pem | \
	    awk 'BEGIN{RS="@@\n"} NF{print > "/tmp/anchor-chk." NR ".pem"}' ; \
	    for f in /tmp/anchor-chk.*.pem; do openssl x509 -in "$f" -noout -fingerprint -sha256 | sed 's/.*=//'; done; rm -f /tmp/anchor-chk.*.pem)
	if [ "$got" = "$want" ]; then echo "anchors: PEM fingerprints are the expected ISRG Root X1 and X2"; else echo "anchors: unexpected certificates in isrg-roots.pem:"; echo "$got"; exit 1; fi
else
	echo "anchors: openssl not found, fingerprints not checked"
fi
