# OS 9 TLS seed provisioning and recovery

The managed-seed build requires an independently generated secret seed. It refuses
TLS if provisioning is missing, invalid or zero-filled, a seed-store lock is already held, or
the next launch's seed cannot be safely saved. It does not silently fall back to
jitter and does not import the legacy raw `weather.seed` file.

## First installation

1. On Linux, run `python3 tools/provision-os9-seed.py --output PATH`, choosing a new
   private path outside the checkout, with filename `weather-seed.import`. The tool
   uses the OS CSPRNG, creates the file exclusively with mode 0600, and never prints
   seed bytes. Never reuse an import for another installation.
2. With the owner's authorization for transferring this secret to that Mac, send
   it through an authenticated private channel into the same folder as SeedInstall.
   On our test machine this is `/Volumes/MacOS9Lives/weather-dev/weather-seed.import`.
   Do not use `maulctl push` for secrets: it explicitly changes the destination mode
   to 0644. The existing `maulctl osx` SSH route can stream input with a restrictive
   remote umask and exclusive creation. Do not put seed bytes in command arguments.
3. Cross-build on Linux with `make os9` and deliver `SeedInstall.dsk` through the
   existing app-delivery helper. Quit Weather and run SeedInstall. Read
   `seed-install.log` beside the utility. It exclusively creates `Weather TLS Seed`
   in the active OS 9 Preferences folder, refuses to overwrite an existing record,
   and removes the import after successful installation. If import removal fails,
   the log says so; remove that extra copy before normal use.
4. Remove the local import copy once successful delivery and installation are
   confirmed. Ordinary deletion is not a guarantee of forensic erasure on modern
   storage; protect the source filesystem and avoid creating unnecessary copies.
5. Launch Weather. It saves a successor seed before releasing TLS entropy, keeps an
   in-memory HMAC_DRBG and the exclusive lock for its lifetime, and wipes/releases
   state on normal shutdown. App updates must not replace any managed seed files.

No import or seed belongs in Git, an application disk image, a shared package,
logs, screenshots or published test fixtures. Diagnostic records are explicitly
synthetic and must never be used to bootstrap a real installation.

## File format and lifecycle

The record is exactly 72 bytes: eight-byte magic/version `WXSEED1\n`, 32 secret seed
bytes, then SHA-256 of the first 40 bytes. The checksum detects accidental corruption;
it neither authenticates the file nor proves that a CSPRNG generated its contents.
Versioning prevents accidental promotion of old jitter-only seed files.
The decoder also refuses a checksum-valid record whose 32 seed bytes are all
zero, without changing its output buffer. The provisioning helper refuses
zero-filled or wrong-length OS random output before creating an import. These
checks detect a placeholder/fault; nonzero bytes do not prove entropy or secrecy.

The OS 9 adapter uses `FindFolder` for Preferences and fixed managed names:
`Weather TLS Seed`, `Weather TLS Next`, `Weather TLS Lock`. The active store does
not depend on the current working directory. Exclusive `FSpCreate` provides lock
ownership; a second app is refused. Lock files are never stolen automatically.

On initialization, HMAC_DRBG is seeded from the managed secret and supplemental
local timing samples. Its first generated block is reserved as the next launch's
seed. The adapter exclusively creates Next, checks write length and close, flushes
the volume, exchanges Seed/Next, flushes, deletes the old record now named Next,
then flushes again. Only after every required operation succeeds may the pool
release output. Later requests advance the in-memory generator rather than reread
the original file. Temporary secret buffers are cleared through volatile stores.

This is a checked replacement protocol, not a claim of power-loss atomicity or
durability. File exchange/flush behavior was tested on maul's OS 9 volume; physical
power loss and snapshot rollback were not tested. Disk/VM clones and backups can
duplicate or roll back seed state. A stolen seed compromises the assumptions;
unmeasured jitter does not prove recovery from that compromise. Classic OS 9 does
not provide modern per-user isolation.

## If seed state is exposed

BearSSL documents that PRNG output depends on seed inputs and the exact sequence
of generation calls (including their lengths):
[BearSSL PRNG documentation](https://www.bearssl.org/apidoc/bearssl__rand_8h.html).
For this application, someone who learns a saved seed and can reconstruct the
supplemental inputs and calls can reproduce the stream. Saving a successor before
releasing TLS output does not recover secrecy after that compromise. The timing
samples have no measured entropy guarantee; differing outputs for differing
samples demonstrate mixing, not resistance to guessing those samples.

A leftover import, backup or cloned volume can expose or duplicate seed state.
After suspected exposure, stop Weather and follow the owner-controlled recovery
procedure below to provision an independent new OS CSPRNG seed. Do not rely on
normal rotation or additional launches to recover secrecy. Protect and remove
all unneeded copies according to the first-install instructions.

The current decoder rejects all-zero seeds, but still accepts known nonzero
diagnostic seed values. That remaining admission hardening is tracked in issue
#59. The checks do not prove the provenance of any accepted record.

## Recovery

The application refuses ambiguous recovery rather than picking an old seed:

| State | Action |
|---|---|
| Missing/invalid Seed | Provision a fresh unique seed using SeedInstall |
| Lock exists | First verify no Weather, SeedInstall or probe owns it; a crash may leave a stale lock |
| Next exists after a failure | Treat rotation as interrupted; do not automatically restore the old record |
| Clone, restored snapshot or suspected seed disclosure | Provision a fresh independent seed |

For stale-lock/interrupted-rotation recovery, quit all users of this seed store.
Remove only the three managed Weather TLS files from Preferences, then provision
a newly generated seed. Do not alter unrelated preferences. Never remove a live
owner's lock or reuse an old import to recover. These actions need no root on the
supported test installation; privileged changes remain the owner's responsibility.

## Verification

- Host: seed record truncation, overlength, corruption at every byte, independent
  HMAC-DRBG vector, failed read/rotation, exclusive ownership, restart continuity,
  output limits and cleanup failures are exercised in unit tests.
- `test/robust.cpp` mutates the binary record alongside existing parser inputs.
- `test/seed_provision.py` checks record/checksum, distinct outputs, mode 0600,
  non-overwrite and rejection of output paths inside the checkout.
- SeedFileProbe on maul passed exclusive create, exchange, flush and readback checks
  using only disposable files.
- SeedInstall correctly refused a missing import on maul, logging
  `FAIL: weather-seed.import missing or invalid`. This is the expected negative test;
  no managed seed was installed by that run.
- With explicit owner approval, a genuine private seed was delivered and installed.
  The first installer run reported import-deletion failure; inspection showed
  Retro68's `_unlink_r` is a stub returning -1. The temporary private import was
  removed through authenticated SSH, and the local import was removed after
  successful verification. The installer now uses native File Manager deletion.
  A synthetic regression first reproduced two failed cleanup assertions, then
  passed on maul with the fix, including both absolute and relative paths. The
  complete storage probe finished with zero failures after the fix.
- SeedStoreProbe on maul passed normal/restart ownership checks and seven injected
  failures in the actual adapter: short write, close, pre-exchange flush, exchange,
  post-exchange flush, old-file deletion and final flush. Every failure released no
  TLS bytes and left a complete primary record. Tests ran in a new disposable folder.
- TlsSeedProbe performs a certificate-verified live Open-Meteo fetch and checks that
  the real managed seed advances, logging only success/error and a boolean. Run it
  twice to check actual process-to-process continuity. Both launches passed on
  maul (2026-10-25), each logging `ok live verified-TLS fetch` and `seed-advanced=1`.
  NetBSD CLI, X11 and test binaries also cross-built successfully; hardware execution
  on NetBSD remains outstanding while maul runs Tiger/Classic.

Build the latter probes with `WEATHER_BUILD_DIAGNOSTICS=ON` in the OS 9 CMake build.
Fault injection is compiled only into SeedStoreProbe, never the Weather app.
No test establishes a quantitative entropy level for the supplemental samples.
