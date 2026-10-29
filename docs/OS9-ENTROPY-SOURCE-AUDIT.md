# Classic Macintosh entropy source audit

2026-10-29; Refs #197, #121, #193, #120 and #65.

Decision: keep Weather's BearSSL and managed seed store. Write a minimal diagnostic
adapter of our own if direct PowerPC time-base observations are useful to #193.
No upstream implementation is imported by this audit. The event-timing pilot
remains Claude's work; this memo neither changes its preregistered symbols nor
selects per-event credit. The owner's v0.1 target is 128 credited bits, but current
measurements still establish zero credit. #197 is Research; #120/#121/#193 remain
v0.1 work, with owner-reviewed hardware evidence outstanding.

## Sources and compatibility

Links pin the actual revisions inspected, rather than tracking branch tips.

| Implementation | Observations / construction | Build and reuse decision |
|---|---|---|
| [macTLS entropy module](https://github.com/mplsllc/macTLS/blob/66b48e9b58e69696718995bc7b3ce0b051be74ac/os9/ostls_entropy.c) | TickCount, Microseconds, changed mouse position, stack address, caller bytes/hints; running SHA-256, distinct extraction tags, output feedback, then BearSSL injection. | [README](https://github.com/mplsllc/macTLS/blob/66b48e9b58e69696718995bc7b3ce0b051be74ac/README.md) targets CodeWarrior 8 C89 / Carbon CFM PPC; Retro68 is syntax preflight. Toolbox names and crypto primitives overlap, but this does not establish Weather's native PEF ABI or runtime compatibility. Study patterns; retain our implementation. |
| [MacSSL README](https://github.com/bbenchoff/MacSSL/blob/dac3dcf9c1c72546fda1a0a055163a4f8601aa12/README.md) and Project sources | README describes clocks, mouse, memory, hardware/network timing, TCP state and user delays combined by XOR. It explicitly disclaims security. | CodeWarrior Pro 4 C89/C90 proof of concept using Mbed TLS/PolarSSL. Not a portable entropy adapter. No crypto-stack or runtime import. |
| [OpenSSL Randomizer.cpp](https://chromium.googlesource.com/chromium/deps/openssl/+/136315fff2b416c62ab74ee8f287b4a57948cbc0/openssl/MacOS/Randomizer.cpp), [header](https://chromium.googlesource.com/chromium/deps/openssl/+/136315fff2b416c62ab74ee8f287b4a57948cbc0/openssl/MacOS/Randomizer.h) | Startup/application times, mouse displacement and stationary duration, disk-volume state, filler and timer readings feed RAND_add. Lower 32 timer bits; separate 601 path. | Compiler-specific assembly, Toolbox/low-memory interfaces and OpenSSL accounting differ from Weather. Reimplement only the small observation seam; do not port the class or its credits. |
| [Classilla archive](https://github.com/classilla/classilla/tree/2bb89318d4af79b376897bebdad1724fc63ee2f2) | This revision contains README and wiki, not the NSS implementation. | No source-grounded claim about Classilla randomness is made. Failed historical NSS raw-file lookups do not verify an implementation. A future comparison needs an actual archived source revision. |

MacSSL's readable Project tree was inspected without extracting Archive.sit or
running any included binary. README refers to SSLWrapper.c, but that application
source was not found in the inspected tree; its custom collector is therefore
**described, not implementation-verified**. Library entropy interfaces and bundled
self-test calls alone cannot establish the application's real seeding path.

## What to retain from the research

macTLS's [scope document](https://github.com/mplsllc/macTLS/blob/66b48e9b58e69696718995bc7b3ce0b051be74ac/MACENTROPY_SCOPE.md)
uses changed output fingerprints and distribution checks as evidence. These can
catch broken output, but deterministic hash chains also vary and look well
spread. They do not establish attacker uncertainty. Its sample counter counts
pool updates, not credited independent events. Its save path deletes/recreates
the seed and does not enforce Weather's checked ownership/rotation contract.
Keep Weather's fail-closed lifecycle, including partial-write/close/flush checks.

The old OpenSSL source itself labels its estimates optimistic. Its filler path
credits predictable system state to satisfy the RNG's readiness requirement.
Neither that shortcut nor its mouse/startup constants belongs in Weather. Its
warning that event dequeue timing includes scheduling delay is directly relevant
to the pilot: fine timer resolution does not make event arrival observable at
that resolution. The code's assumed CPU-to-timer ratio is not a G4 calibration.

## PowerPC timer experiment and pilot recommendation

[NXP's MPC7450 family manual, revision 5](https://www.nxp.com/docs/en/reference-manual/MPC7450UM.pdf),
sections 2.2.4.1 and 2.4.5.1, documents user-level time-base reads and separate
upper/lower halves. This supports a **compile experiment**, not a claim that
Classic or native OS 9 on maul has been tested. No privileged register access,
601 compatibility or CPU-frequency-derived conversion is proposed.

An independently written scratch function reads upper/lower/upper, retries when
the upper half changes, and assembles a 64-bit value. Retro68 C++17 compilation
with -Werror, -fno-exceptions and -fno-rtti passed. Object disassembly contains
mftbu/mftb/mftbu, comparison/retry and return; nm reports no undefined symbols.
This checks compiler syntax and object generation only: no executable link,
PEF launch, monotonicity, counter frequency, virtualization or noise was tested.
The scratch experiment is outside the repository and is not linked into Weather.

If #193 adds this optional observation in a separate diagnostic PR:

1. Bound upper-half retries and mark failure invalid. Host-test rollover,
   invalid/regressing readings and interval arithmetic with synthetic values.
2. Store raw counter units separately from Microseconds and EventRecord.when.
   Never apply the pilot's microsecond symbol rules to uncalibrated counter ticks.
   Any new mapping must be declared before observing the new dataset.
3. Compare ratios, monotonicity and resolution separately under Classic/native,
   including pauses and event backlogs. Do not infer frequency from CPU MHz.
4. Treat all these clocks as correlated observations of one event, not additive
   independent sources. Timers are instruments; human event timing is the
   candidate source. Auto-repeat, scripted input and every mouse poll gain no
   automatic credit. Keep idle/metronome/scripted controls and session boundaries.
5. Keep collection outside OT callbacks, omit key identities/content and stable
   machine identifiers, and preserve diagnostics-ON/OFF exclusion checks. No seed
   reads, production collector or threshold change follows from this experiment.

## Attribution and import boundary

macTLS's [MIT license](https://github.com/mplsllc/macTLS/blob/66b48e9b58e69696718995bc7b3ce0b051be74ac/LICENSE)
names Patrick Britton (2026); its [third-party notices](https://github.com/mplsllc/macTLS/blob/66b48e9b58e69696718995bc7b3ce0b051be74ac/THIRD_PARTY_NOTICES.md)
separate project-authored code from BearSSL and Certainly adaptations. A future
small adaptation requires the applicable copyright/permission text in the source
and distribution notices, plus a file-specific provenance check. The root license
must not be assumed to cover every bundled component.

Inspected MacSSL library files carry their own Mbed TLS notices, including
Apache-2.0 OR GPL-2.0-or-later headers. That does not establish permission for an
unlocated application collector. The historical OpenSSL tree's
[LICENSE](https://chromium.googlesource.com/chromium/deps/openssl/+/136315fff2b416c62ab74ee8f287b4a57948cbc0/openssl/LICENSE)
has the old OpenSSL and SSLeay conditions; Randomizer.cpp/.h provide no separate
permission notice. Do not classify them as modern Apache-2.0 or copy them based
on a generic compatibility assumption. This audit avoids that import question.
Apple headers/binaries stay outside Weather's repository.

Research acknowledgment (distinct from mandatory copied-code notices):

> Entropy research consulted Patrick Britton's macTLS/macEntropy, Brian
> Benchoff's MacSSL, and the historical OpenSSL Macintosh randomizer. These
> informed experiment design; their entropy implementations were not imported.

This memo supplies that acknowledgment. THIRD_PARTY.md remains the record of
components actually distributed, so this research does not add a dependency claim.

## Reproduction and limits

Upstream clones and original line endings remain under
/tmp/weather-entropy-audit/{macTLS,MacSSL}. The OpenSSL source was retrieved as
base64 through the pinned Gitiles format=TEXT endpoint and decoded outside the
checkout. No private Mac data or seed material was accessed. Scratch timer source,
object and compile/disassembly log are under /tmp/weather-entropy-audit/ppc and
/tmp/weather-entropy-audit/timebase-compile.log. These /tmp paths are ephemeral;
the revisions, findings and limitations above are the durable record.

OpenSSL Randomizer.cpp SHA256:
8ed1d1c2faf6bbbac75046517ce7a7688525893065c14b81cc6c63ac4901b8ef

No hardware measurements were performed. The source audit cannot establish a
per-event credit, scripted-input detection guarantee, cold-boot independence,
other-model behavior or power-loss durability. The existing provisioning policy
and zero-credit measurement reports remain unchanged.
