# Isolated test fixtures

Feature and GUI fixtures bind loopback port 0 and announce the kernel-selected
port only after listening. Each invocation owns a private temporary directory,
PID list and readiness files. `test/wait-ready.py` waits for an announced decimal
port/display from a live child, with a ten-second monotonic startup limit. It
rejects malformed announcements and dead children. A startup failure stops the
suite before a weather client runs; it cannot silently use another suite's server.
There is no shared port range or unconditional fixture startup sleep. The old
internal PORT_BASE override is no longer needed. The server still accepts an
explicit port for standalone diagnostics.

Host GUI tests use Xvfb's `-displayfd` allocation, wait for its announced display,
and retain the server with `-noreset` until the harness cleans up its own PID.
The shared scenario can use that prestarted display; legacy remote execution
still starts its own server. `-nolisten tcp` is retained.

Run all GUI checks serially, including private displays, per the owner’s working
agreement. Concurrent non-GUI checks require separate source/build/output trees,
such as separate worktrees or source copies. Do not run two compilers into the same object
directory or change source while a compiler may read it. Merely overriding BUILD
is insufficient for isolating the GUI output paths. Older branches with fixed
ports/display :88 still need coordination until they adopt this change.

The maul GUI run remains serial on :77. Its fixtures use ephemeral local ports
and reverse SSH forwarding fails closed if any remote bind fails. Each run owns
a validated `/tmp/wx-gui.XXXXXXXX` directory, removes it on success, and keeps it
on failure. It never removes the shared `/tmp/wx` directory. Linux performs all
compilation; maul runs the cross-built programs only.

`make check` runs three fixture-startup regressions: two simultaneous live servers
have distinct announced ports and independent responses; malformed/stale readiness
is refused; and forced fixture startup failure prevents invoking the client.
