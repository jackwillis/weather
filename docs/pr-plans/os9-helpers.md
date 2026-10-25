# OS 9 portable helpers

Status: draft plan only; implementation and tests have not started.

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
