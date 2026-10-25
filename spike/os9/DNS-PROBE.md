# Asynchronous DNS probe

Build DnsProbe from this CMake project with the Retro68 PowerPC toolchain on
Linux; it links only our own OT client glue and Retro68 import libraries.
The verified async-open glue is now also used by Weather.

Deliver DnsProbe.dsk using tools/deliver-os9.sh with app name DnsProbe. Run only
with the owner's Classic console login available. The log is
MacOS9Lives:weather-dev:dns-probe.log. No clicking is needed.

The notifier publishes fixed event records with compiler acquire/release atomics;
there is no file I/O, Toolbox drawing or allocation in callback context. Each
case has permanent storage, retained until after OT shutdown. The probe logs
immediate completion, query cookie/result, pending status before close, close
duration, foreground pumps, and callbacks observed after close. It checks numeric
and weather-host resolution, an invalid domain, close immediately after starting
a fresh query, and a new query after close. It does not alter system DNS settings.

An immediate close is a cancellation *mechanism probe*, not an implementation of
Net cancellation. A fast cached/NXDOMAIN response can leave no pending query to
cancel: that coverage is explicitly reported incomplete. A one-second observation
without callbacks is evidence only, not proof of callback quiescence. This probe
does not cover a delayed open, a controlled DNS timeout, or arbitrary callback
ordering. Host lifecycle tests and a delayed DNS fixture are required before
full hardware acceptance. The probe itself never accesses the seed file.

## Results on maul, 2026-10-25

DnsProbe: zero failed assertions, 103 event pumps. Numeric and cached weather-host
queries completed before the call returned; a fresh invalid domain completed later
with error -3170. A unique invalid query was still pending when closed, and close
took zero ticks. No callback was observed in the following one-second window.
Resolution after that close succeeded.

DnsNetProbe (optional WEATHER_BUILD_DIAGNOSTICS in os9/CMakeLists.txt) uses the
production backend and controller, without TLS/seed access. All five cases passed:
weather-host TCP connection, invalid-host DNS error, cancellation before open,
cancellation during lookup, then another weather-host connection. Cancellation
returned in zero ticks in that run. Controller host tests cover timeout, clock wrap,
close failure quarantine, pending-open retention/drain and boundary precedence.
make check and make check-san passed: 264 unit, 164576 robustness inputs, 78 CLI;
the normal host check also passed all 21 GUI assertions.

Still incomplete: deliberately stalled DNS fixture, pending-open cancel on hardware,
and a proof stronger than the API contract plus bounded observation for no callback
after close. Older prose in ARCHITECTURE.md describing synchronous DNS is historical.

Public API references used for our own declarations:

- https://dev.os9.ca/techpubs/mac/OpenTransport/OpenTransport-151a.html
- https://dev.os9.ca/techpubs/mac/OpenTransport/OpenTransport-152a.html
- https://dev.os9.ca/techpubs/mac/NetworkingOT/NetworkingWOT-126.html
- https://leopard-adc.pepas.com/documentation/mac/NetworkingOT/NetworkingWOT-105.html

The newer close reference says outstanding async calls do not complete after
close; older documentation warns clients about outstanding operations. Treat
production cleanup as unverified until the specific target has been tested.
