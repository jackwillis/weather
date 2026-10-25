#!/bin/sh
# 1. src/tls/anchors.h must be exactly what the PEM generates (nobody hand-edited it).
# 2. The PEM must contain exactly the roots we expect (SHA-256 fingerprints below).
# usage: tools/check-anchors.sh BRSSL
BRSSL=${1:?usage: check-anchors.sh path/to/brssl}
cd "$(dirname "$0")/.." || exit 1
tmp=$(mktemp)
sh tools/gen-anchors.sh "$BRSSL" 2>/dev/null > "$tmp"
if cmp -s "$tmp" src/tls/anchors.h; then echo "anchors: anchors.h matches roots.pem"; else echo "anchors: anchors.h is out of date (run: make anchors)"; rm -f "$tmp"; exit 1; fi
rm -f "$tmp"
# ISRG Root X1, ISRG Root X2 (Open-Meteo, api.weather.gov), HARICA TLS RSA Root CA 2021 (api.met.no)
want="96:BC:EC:06:26:49:76:F3:74:60:77:9A:CF:28:C5:A7:CF:E8:A3:C0:AA:E1:1A:8F:FC:EE:05:C0:BD:DF:08:C6
69:72:9B:8E:15:A8:6E:FC:17:7A:57:AF:B7:17:1D:FC:64:AD:D2:8C:2F:CA:8C:F1:50:7E:34:45:3C:CB:14:70
D9:5D:0E:8E:DA:79:52:5B:F9:BE:B1:1B:14:D2:10:0D:32:94:98:5F:0C:62:D9:FA:BD:9C:D9:99:EC:CB:7B:1D"
if command -v openssl >/dev/null 2>&1; then
	# Split the PEM into one file per certificate with plain sh: awk dialects differ (BSD awk rejects the gawk idiom).
	d=$(mktemp -d) || exit 1
	n=0; inside=0
	while IFS= read -r line; do
		case $line in
		*"BEGIN CERT"*) n=$((n+1)); inside=1; : > "$d/$n.pem";;
		esac
		[ "$inside" = 1 ] && printf '%s\n' "$line" >> "$d/$n.pem"
		case $line in
		*"END CERT"*) inside=0;;
		esac
	done < src/tls/roots.pem
	got=$(i=1; while [ "$i" -le "$n" ]; do openssl x509 -in "$d/$i.pem" -noout -fingerprint -sha256 | sed 's/.*=//'; i=$((i+1)); done)
	rm -rf "$d"
	if [ "$got" = "$want" ]; then echo "anchors: PEM fingerprints are the expected ISRG Root X1, ISRG Root X2 and HARICA TLS RSA Root CA 2021"; else echo "anchors: unexpected or missing certificates in roots.pem:"; echo "$got"; exit 1; fi
else
	echo "anchors: openssl not found, fingerprints not checked"
fi
