# OS 9 portable helpers

Status: implemented; verification results below.

The OS 9 backend now uses pure helpers for signed 24-bit timezone decoding,
Mac-to-Unix UTC conversion, full-width signed/unsigned decimal formatting, and
bounded log-format adaptation. The waiting/debug log no longer passes `%lld` to
Retro68; the console clock no longer narrows a timestamp to unsigned long.
Oversized or incomplete format strings fail without passing a truncated format
to the variadic formatter. Literal percents and `%ll` are preserved.

Verification:
- Host unit regressions cover timezone sign boundaries, high-byte/DST handling,
  epoch boundaries, integer limits, exact-fit/truncated buffers, and formats.
- Format mutations run through the robustness harness, with guard bytes and
  independent integer formatting comparisons against the host formatter.
- `make check` passed: 489 unit checks, 206820 robustness inputs, 78 CLI checks,
  21 X11 GUI checks and 2 provisioning tests. After the final mutation assertions,
  `make check-native` and `make check-san` passed again; the sanitizer log confirms
  AddressSanitizer as well as UBSan.
- Native OS 9 cross-build and PowerPC/68k compile-time checks pass.
- PortableProbe ran on maul under Classic: rewritten `%08zu` printed correctly,
  signed minimum/unsigned maximum printed exactly, zero failures. Its UTC sample
  1792957340 fell between surrounding Tiger readings 1792957339 and 1792957369.
  This probe is seed-free and self-terminating; it does not test network/TLS.
- Local 32-bit runtime checks could not compile because `gnu/stubs-32.h` is
  absent. PowerPC/68k compilation is verified; 32-bit runtime remains unverified.

Scope: extract UTC conversion and log-format handling into OS-independent helpers;
add bounded signed/unsigned 64-bit decimal formatting and replace remaining OS 9
%lld calls with %s. Do not narrow timestamps or copy Apple headers. Preserve the
hardware finding that gmtDelta already includes daylight saving.

Acceptance:
- Failing host regressions first for signed 24-bit timezone offsets, DST flag
  interpretation, 1904/1970 epoch conversion, zero and integer limits.
- Verify literal percent signs, size_t format conversions, flags/width/precision
  actually supported by callers, truncation and NUL termination. Mutate any new
  format-input parser; do not turn %ll into a 32-bit conversion.
- Compile static tests for host, 32-bit and PowerPC/Retro68; run sanitizers.
- Compare the app's UTC clock with maul's OS X clock and inspect representative
  real log output. Keep platform API access outside the portable helpers.

Dependencies: merged OS 9 backend; independent of the window layout. Coordinate
shared config extraction with #11 and #12 to avoid duplicate implementations.
