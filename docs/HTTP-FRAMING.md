# HTTP framing and TLS closure

The HTTP/1.0 client accepts Content-Length only as nonempty decimal digits that
fit size_t. Zero and leading zeros remain valid. Equal repeated fields are
accepted; conflicting values and comma lists are refused. No transfer coding is
implemented, so any recognized Transfer-Encoding field is refused regardless of
name/value case. This is a deliberately limited client, not a complete HTTP/1.1
implementation.

The response buffer includes headers and body. Filling it exactly is not an
error: one bounded extra-byte read distinguishes clean EOF from oversized data,
while preserving transport failures. The byte is never written past the buffer.

TLS EOF is clean only with authenticated closure. A transport EOF without
close_notify is Truncated, including for close-delimited HTTP, and normal retry
policy applies. This deliberately also refuses an unclean close after a complete
Content-Length body. Explicit raw errors such as timeout/cancellation retain their
existing mapping. Certificate error classification is unchanged.

The TLS fixture now has proper close_notify controls, deliberate complete/cut
abrupt closes, and a three-attempt retry case. --attempts is a named harness flag
to avoid colliding with #41's positional deadline argument during integration;
that merge must preserve both interfaces and #41's raw-error message forwarding.

Primary references checked 2026-10-26:
- [RFC 9110 section 8.6](https://www.rfc-editor.org/rfc/rfc9110.html#section-8.6): decimal length syntax and overflow protection.
- [RFC 9112 sections 7 and 8](https://www.rfc-editor.org/rfc/rfc9112.html#section-7): coding names and incomplete responses.
- [RFC 5246 section 7.2.1](https://www.rfc-editor.org/rfc/rfc5246.html#section-7.2.1): TLS 1.2 closure alerts.

Host offline TLS and real Open-Meteo on NetBSD/macppc passed. Live MET Norway,
NWS, geocoding and native OS9 closure compatibility were not rerun here.
