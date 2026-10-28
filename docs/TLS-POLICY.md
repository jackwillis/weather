# TLS policy

Weather negotiates TLS 1.2 only. Its explicit suite list uses ECDHE with either
RSA or ECDSA authentication and one of ChaCha20-Poly1305, AES-128-GCM or
AES-256-GCM. It offers no static RSA key exchange, CBC, 3DES, TLS 1.0 or TLS 1.1.
Certificate validation still uses the compiled trust anchors, requested hostname
and platform UTC clock. This change does not add a new certificate key-size or
signature-algorithm policy.

Production binaries ignore `WEATHER_NOW` and `WEATHER_NO_TA`. Build with
`make TEST_HOOKS=1` only for diagnostics that deliberately override time or trust.
Those artifacts live under `build/<target>-test-hooks/`, with separate objects
and libraries. The build refuses installation, packaging and distribution in
that mode. The normal `make check` and `make check-san` exercise both production
and test-hook configurations, and inspect linked binaries for hook strings.
The separate `WEATHER_TEST_ANCHORS` TLS fixture mechanism remains compile-only.

The offline TLS suite checks certificate errors and rejects TLS-1.1-only and
CBC-only peers before any HTTP request reaches them. The CBC-only peer was
accepted before the policy change, providing a failing regression test.

For an optional live compatibility check, build `make build/host/tls-live` and
run `build/host/tls-live HOST`. It uses the production TLS implementation and
anchors, drives a handshake without sending HTTP bytes, and logs the negotiated
version and suite. The 2026-10-27 run passed Open-Meteo, MET Norway, NWS and the
Open-Meteo geocoding service. This proves handshake compatibility at that time;
it does not claim API request/response acceptance or native hardware execution.
See `verification/tls-policy-2026-10-27.txt`.
