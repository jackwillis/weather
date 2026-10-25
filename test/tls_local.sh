#!/bin/sh
# Offline test of certificate verification: the real tls.cpp, the real fetch_once and net_posix, a local TLS server,
# and certificates made here with openssl. The only difference from production is the trust anchor: tls.cpp is
# rebuilt with -DWEATHER_TEST_ANCHORS so it trusts a throwaway CA instead of the compiled-in roots.
#
# Each bad certificate must be refused during the handshake, with the right BearSSL reason, before a single request
# byte is sent (the server logs every request it reads, and must log none). A good certificate is the control.
#
# usage (from the repo root; make check-tls):  sh test/tls_local.sh CXX BUILD_DIR BRSSL [CXXFLAGS [LDFLAGS]]
# e.g.  sh test/tls_local.sh c++ build/host build/host/bearssl/brssl
# CXX may carry flags (c++ -m32); BRSSL is a brssl that runs on this machine (the host one, even for a 32-bit target);
# CXXFLAGS/LDFLAGS: the target's extra flags (sanitizers), so the harness links with the objects it reuses.
CXX=${1:?usage: tls_local.sh CXX BUILD_DIR BRSSL [CXXFLAGS [LDFLAGS]]}
B=${2:?usage: tls_local.sh CXX BUILD_DIR BRSSL [CXXFLAGS [LDFLAGS]]}
BRSSL=${3:?usage: tls_local.sh CXX BUILD_DIR BRSSL [CXXFLAGS [LDFLAGS]]}
XCXXFLAGS=${4:-}
XLDFLAGS=${5:-}
unset WEATHER_NO_TA     # that test hook empties the trust set, which would fail the control
pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "FAIL: $1"; }

command -v openssl >/dev/null 2>&1 || { echo "tls tests: openssl not found, skipped"; exit 0; }
command -v python3 >/dev/null 2>&1 || { echo "tls tests: python3 not found, skipped"; exit 0; }

T=; SRV=
trap 'kill $SRV 2>/dev/null; rm -rf "$T"' EXIT
trap 'exit 1' INT TERM
T=$(mktemp -d "${TMPDIR:-/tmp}/tls-test.XXXXXX") || exit 1

# ---- certificates: a trusted root, an unrelated root, and leaves with the properties under test
cat > "$T/ca.cnf" <<'EOF'
[req]
distinguished_name = dn
x509_extensions = v3
prompt = no
[dn]
CN = unused
[v3]
basicConstraints = critical,CA:TRUE
keyUsage = critical,keyCertSign,cRLSign
EOF
printf 'basicConstraints = CA:FALSE\nsubjectAltName = DNS:localhost\n' > "$T/ext_good"
printf 'basicConstraints = CA:FALSE\nsubjectAltName = DNS:other.test\n' > "$T/ext_other"
printf 'basicConstraints = critical,CA:FALSE\n' > "$T/ext_nonca"

newkey() { openssl ecparam -name prime256v1 -genkey -noout -out "$1" 2>/dev/null; }
mkca() {      # mkca NAME CN : self-signed CA, 10 years
	newkey "$T/$1.key" &&
	openssl req -new -x509 -key "$T/$1.key" -subj "/CN=$2" -days 3650 -config "$T/ca.cnf" -out "$T/$1.pem" 2>/dev/null
}
mkcert() {    # mkcert NAME CN ISSUER EXTFILE DAYS : certificate signed by ISSUER
	newkey "$T/$1.key" &&
	openssl req -new -key "$T/$1.key" -subj "/CN=$2" -config "$T/ca.cnf" -out "$T/$1.csr" 2>/dev/null &&
	openssl x509 -req -in "$T/$1.csr" -CA "$T/$3.pem" -CAkey "$T/$3.key" -CAcreateserial -days "$5" -extfile "$T/$4" -out "$T/$1.pem" 2>/dev/null
}
if ! { mkca ca "Test Root" && mkca other "Other Root" &&
       mkcert good good ca ext_good 30 && mkcert wronghost wronghost ca ext_other 30 &&
       mkcert untrusted untrusted other ext_good 30 &&
       mkcert inter inter ca ext_nonca 30 && mkcert viainter viainter inter ext_good 30 &&
       cat "$T/viainter.pem" "$T/inter.pem" > "$T/viainter_chain.pem"; }; then
	bad "could not generate test certificates with this openssl"
	echo "tls tests: $pass passed, $fail failed"; exit 1
fi

# ---- the client under test: tls.cpp with the throwaway root as its only trust anchor
"$BRSSL" ta "$T/ca.pem" > "$T/test_anchors.h" 2>/dev/null || { bad "brssl could not convert the test root"; echo "tls tests: $pass passed, $fail failed"; exit 1; }
FLAGS="-std=c++17 -O1 -fno-exceptions -fno-rtti -Isrc -I$T -Ithird_party/bearssl/inc $XCXXFLAGS"
if ! { $CXX $FLAGS -DWEATHER_TEST_ANCHORS -c src/tls/tls.cpp -o "$T/tls.o" &&
       $CXX $FLAGS -c test/tls_harness.cpp -o "$T/harness.o" &&
       $CXX $XLDFLAGS -o "$T/harness" "$T/harness.o" "$T/tls.o" "$B/obj/core/fetch.o" "$B/obj/platform/posix/net_posix.o" \
           "$B/obj/platform/posix/log_posix.o" "$B/bearssl/libbearssl.a"; }; then
	bad "could not build the harness"
	echo "tls tests: $pass passed, $fail failed"; exit 1
fi

# ---- servers: one port per case, each with its own chain
python3 -B test/tls_server.py "$T/ports" "$T/requests" \
	"good:$T/good.pem:$T/good.key" "wronghost:$T/wronghost.pem:$T/wronghost.key" \
	"expired:$T/good.pem:$T/good.key" "notyet:$T/good.pem:$T/good.key" \
	"untrusted:$T/untrusted.pem:$T/untrusted.key" "nonca:$T/viainter_chain.pem:$T/viainter.key" &
SRV=$!
i=0; while [ ! -s "$T/ports" ] && [ $i -lt 20 ] && kill -0 $SRV 2>/dev/null; do sleep 1; i=$((i+1)); done
[ -s "$T/ports" ] || { bad "the test server did not start"; echo "tls tests: $pass passed, $fail failed"; exit 1; }

NOW=$(date +%s)
# check NAME CLOCK WANT_ERR WANT_DETAIL WANT_REQUESTS_SEEN_BY_SERVER
check() {
	port=$(sed -n "s/^$1 //p" "$T/ports")
	OUT=$("$T/harness" localhost "$port" "$2" 2>&1)
	want="RESULT err=$3 detail=$4"
	if [ "$OUT" = "$want" ]; then ok; else bad "$1: got '$OUT', wanted '$want'"; fi
	seen=$(grep -c "^$1\$" "$T/requests")
	if [ "$seen" = "$5" ]; then ok; else bad "$1: the server read $seen request(s), wanted $5"; fi
}

check good      "$NOW"                    ok   0               1    # control: everything right
check wronghost "$NOW"                    cert BAD_SERVER_NAME 0    # certificate is for another name
check expired   $((NOW + 60 * 86400))     cert EXPIRED         0    # clock 60 days on; the leaf lasts 30
check notyet    1750000000                cert EXPIRED         0    # clock in mid-2025, before the leaf existed
check untrusted "$NOW"                    cert NOT_TRUSTED     0    # signed by a root we do not trust
check nonca     "$NOW"                    cert NOT_CA          0    # leaf signed by an intermediate that is not a CA

echo "tls tests: $pass passed, $fail failed"
[ "$fail" = 0 ]
