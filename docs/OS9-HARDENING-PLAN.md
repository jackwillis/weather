# OS 9 hardening plan

Proposed 2026-10-25. Planning only; no implementation or hardware verification implied.
Read with AGENTS.md, STATUS.md and ARCHITECTURE.md. Jack's requested order takes
precedence over the older UI-first task order in the handoff.

## Scope and milestones

1. Establish a host baseline and preserve the existing shared-artwork changes.
2. Prove asynchronous DNS lifecycle behavior on OS 9, then integrate and test it.
3. Add provisioned TLS seed handling with checked persistence and failure tests.
4. Build the first OS 9 window, then dialogs and persistent preferences.
5. Complete hardware regressions and release documentation.

Disk caching, NWS, geocoding and native Tiger support remain deferred. Keep the
console build as a diagnostic tool. No remote publication; local commits only
at green milestones, with the required co-author line.

## Baseline

Review the worktree before editing; do not overwrite or silently include another
agent's changes. Run make check and make check-san sequentially. Check for an
existing GUI test before using display :88 or fixture ports; do not terminate
unrelated processes to free them. Record failures with their output and distinguish
environment problems from product defects. The earlier GUI failure is unresolved;
the handoff's collision warning is a lead, not proof of its cause.

## DNS: behavior and design

Keep the existing Net interface and synchronous calling convention for callers.
Internally resolve asynchronously while the foreground loop pumps Net::yield.
Return Err::Dns for resolver failures, Err::Timeout for a deadline, and
Err::Cancelled for cancellation. Cancellation must not trigger provider fallback.
Successful resolution continues through the existing TCP and TLS paths, retaining
the original hostname for SNI and certificate verification.

Use one fixed-lifetime resolver operation at a time, including hostname storage,
InetHostInfo, provider reference, completion/result fields and lifecycle state.
Do not give OT pointers to a returning stack frame. A notifier records completion
only: no drawing, file I/O, logging, allocation or recursive fetches from it.
Verify the required callback ABI, visibility and synchronization rules rather than
assuming a volatile flag is sufficient.

Suggested states: Idle, Opening, Resolving, Closing, Done. Cancellation/timeout
latches a terminal reason and begins cleanup; subsequent completion cannot turn
it into success. Do not reuse operation storage until callback quiescence is
established. A generation number is an extra guard, not a substitute for keeping
OT-owned buffers alive. Handle completion during the initiating call as well as
later completion.

Use a wrap-safe elapsed timer for deadlines; the current TickCount-to-milliseconds
conversion does not itself handle TickCount wrap. Start the DNS budget before
opening Internet Services. Keep the existing TCP timeout policy for this slice;
do not silently redefine the whole fetch timeout across retries and addresses.
Check cancellation before starting work and on each foreground polling slice.

### Required probe before integration

Cross-build a small diagnostic on Linux. Verify available OT exports and our own
minimal declarations for asynchronous Internet Services open, notifier setup,
resolution completion, cancellation/close and shutdown. Consult public API docs;
copy no Apple headers or binaries. Do not assume closing a provider immediately
stops callbacks, or that synchronous open/close is bounded.

On maul, demonstrate open/resolve success, unknown host, delayed completion,
cancellation and close with work pending, immediate retry, and shutdown. Confirm
that all pending opens/providers are reclaimed, including a provider delivered
after cancellation. Record callback order and resource ownership without logging
from the notifier. If cleanup cannot be safely bounded, document that result and
revise the design before claiming responsive cancellation.

### DNS acceptance tests

Use a narrow fake OT adapter and fake clock on the host to drive the production
lifecycle code. Assert results, callback/storage lifetime, yield calls, and resource
counts rather than just final state names.

| Case | Required result |
|---|---|
| Success, including immediate completion | Correct addresses; one cleanup; hostname preserved |
| Open/init/query failure; empty address list | Appropriate error; no TCP attempt or leaked provider |
| Deadline before/during open or query | Timeout; no later success or buffer reuse |
| Cancel before start or while pending | Cancelled; no retry/fallback; foreground stays responsive |
| Completion at deadline/cancel boundary | Deterministic foreground policy: observed cancellation first, then expired deadline, then completion |
| Duplicate, late or obsolete completion | No stale result applied; no double close/use after return |
| Retry and shutdown after failure/cancel | No callbacks into invalid state or resource growth |
| Timer wrap and delayed foreground scheduling | Correct expiry without busy spinning |

Host sanitizers exercise hostile callback orderings. Add scripted OS 9 probe
assertions for completion counts and yields. Use a controlled delayed resolver
fixture for actual pending DNS; a cached name or fake host adapter cannot prove
OT cancellation. If that fixture is unavailable, report pending-DNS hardware
coverage as incomplete. Target cancellation observation within two normal polling
slices (about 100 ms, excluding OS scheduling); measure cleanup separately.

## TLS seeds: behavior and design

Use a unique 32-byte seed generated by the Linux OS CSPRNG for each installation.
Provision through the existing authenticated transfer route, never via an
unauthenticated network entropy endpoint. Keep seeds out of Git, disk images,
logs, test artifacts and shared packages. Tests use conspicuously synthetic data.
An existing jitter-generated weather.seed is not evidence of strong provisioning.

Use an explicit new seed-record version/path to distinguish the managed lifecycle
from the legacy file. This records provenance supplied by installation; it cannot
detect a dishonest, copied or predictably generated seed. Recommend a per-user
Preferences location shared by UI and console; do not depend on current directory.
Verify OS 9 file APIs before selecting the replacement mechanism.

Seed a persistent in-process BearSSL HMAC_DRBG with provisioned material and
supplemental local samples; use separate generator outputs for TLS entropy and
the next persisted seed. Remove the custom hash/counter expansion. Fresh samples
are supplemental: do not claim a measured entropy level for jitter. BearSSL's TLS
engine already has a generator; the platform generator maintains seed continuity
across calls and launches.

Before releasing TLS entropy after initialization, successfully replace the saved
seed with its successor. Check read length, format, writes, flush, close and the
replacement result. Never truncate the only valid seed in place. Refuse TLS when
provisioning is absent/invalid or required initial rotation fails; give a clear
OS 9 diagnostic through the existing entropy-failure path. Do not add a silent
jitter-only fallback. Document this intentional first-run behavior change.

Define recoverable old/temp/new file states using the actual OS 9 replacement
guarantees. Until those are verified, do not call replacement atomic or durable.
Persist the next launch's seed before the current launch uses its output, then
advance in-memory state for subsequent requests. Reject a second process using
the same seed store, or prove an exclusive-ownership mechanism; UI and console
must not race. Clear transient secret buffers with a compiler-resistant helper.

Threat boundary: this improves unpredictability when the initial seed is secret.
It does not protect against local seed theft, cloned installations, snapshot
rollback or proven compromise of generator state. Fresh jitter does not establish
recovery from compromise. A classic OS 9 machine lacks modern per-user isolation.

### Seed acceptance tests

Put lifecycle policy behind injectable seed storage, supplemental-sample and
generator seams so host tests exercise production decisions. Link BearSSL into
the appropriate host test target for integration vectors.

| Case | Required result |
|---|---|
| Valid provisioned record | Rotate successfully before output; exact requested byte count |
| Missing, legacy, short, oversized, malformed or unsupported record | Refuse TLS; actionable diagnostic; no secret output |
| Read error, short write, flush/close/replace failure | Refuse initialization; preserve recoverable seed state |
| Crash at every persistence boundary | Defined restart outcome; no success claim for incomplete rotation |
| Repeated calls and simulated relaunch | State advances; persistence ordering verified |
| Concurrent initialization | One owner; no duplicate state consumption |
| BearSSL known-answer vectors | Exact expected bytes for synthetic deterministic seeds |
| Invalid output lengths and buffer boundaries | Defined behavior; no overflow, repetition from counter wrap or partial success |
| Logs and failure paths | No seed bytes logged; temporary secrets cleared |

Add malformed-record mutation coverage in test/robust.cpp if a record parser is
introduced. Output inequality is only a regression check, not a randomness proof.
On OS 9 verify first provisioning, two launches, missing/invalid record, forced
save failure in a disposable test directory, exclusive ownership, and a real
certificate-verified TLS fetch. Never corrupt the owner's active seed for tests.
Verify that delivery/install does not replace a valid seed on app updates.

## Window UI and completion gates

After hardening, implement window/artwork/status, menus and bounded event pumping
through Model and Net::yield. Script refresh, close-during-fetch, redraw and several
weather conditions; inspect screenshots. Add Location and Preferences persistence
in a separate slice with invalid-input/save-failure/restart checks. Include a
portable 64-bit decimal formatter tested at signed/unsigned limits, replacing the
remaining OS 9 %lld log call without narrowing values.

Each implementation slice starts with a failing meaningful test. Run make check
and make check-san sequentially, cross-build OS 9 on Linux, and record hardware
evidence before declaring an OS 9 feature complete. Run NetBSD hardware regression
checks when maul is booted there; ask Jack for required reboots/console login.
No compilation or privileged changes on maul. Update STATUS/README with actual
results and remaining limits. Human review of the real display and privileged
package reinstall remain Jack's steps.

## Open questions resolved by probes, not assumptions

- OT callback ABI, completion events, cancellation/close quiescence and bounded cleanup.
- OS 9 seed-store replacement/durability and exclusive ownership APIs.
- Availability of a controlled DNS fixture and the correct maul boot/login state.
- Whether an OS-provided cryptographic entropy source is actually available on
  the supported OS 9 installation; provisioned seeds remain the proposed baseline.

Reference starting points (verify exact APIs during implementation):
[Apple Open Transport provider modes](https://dev.os9.ca/techpubs/mac/OpenTransport/OpenTransport-24.html)
and [BearSSL random-generator API](https://bearssl.org/apidoc/bearssl__rand_8h.html).
