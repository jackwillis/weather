# RFC: A modern systems library for classic Macintosh

**Status:** Proposal — discussion only  
**Date:** 2026-10-25  
**First target:** Classic Mac OS 9, PowerPC, Retro68  
**Reference application:** `weather`

## Manifesto

**Classic Mac OS is an old runtime, not a requirement to use old development practices.**

It should be possible to write new, reliable software for a classic Macintosh using a modern editor, modern C++, modern build automation and tests on Linux, and a cross-compiler—while producing a *genuine* Macintosh application that respects the machine's APIs and constraints.

The challenge is not to disguise classic Mac OS as a deficient Unix. Its event model, Toolbox APIs, Open Transport, resource forks and cooperative execution are distinctive parts of the environment. A good library makes these facilities easier to use safely, without pretending the Macintosh is POSIX or forcing authors to reproduce decades-old development workflows.

We want a small collection of proven systems facilities that makes another native Macintosh application considerably easier to write than the first.

**This is explicitly Macintosh-first.** Supporting NetBSD or Linux is useful for developing, testing and sharing *implementation concepts*; it is **not** a promise to turn the library into an equal-platform portability framework. Classic Macintosh behavior, performance, integration and reliability decide the public API.

## Why now?

The `weather` project has already exercised a modern C++17 portable core against native OS 9 code: Open Transport, TLS via BearSSL, time conversion, cooperative cancellation, classic file handling and Toolbox event integration. That is enough real application experience to investigate extraction, but not enough to declare every internal seam a public API.

Existing work deserves study before duplication:

- **Retro68:** the essential modern cross-compilation toolchain. Build on top of it rather than replacing it.
- **Clapkit:** a modern C++ effort around classic GUI primitives and application structure. Avoid competing on widgets before identifying an actual gap.
- **macTLS and Certainly:** existing native TLS work with differing compiler/ABI and integration choices. Investigate reuse and compatibility; don't create a new cryptographic protocol implementation.
- **Classic Macintosh Toolbox and Open Transport:** preserve native semantics and platform-specific failure modes.

The distinctive contribution could be **small, tested, modern Retro68-friendly systems components**, especially safe Open Transport lifecycle and cancellation, storage and clock helpers, without requiring a GUI toolkit.

Links and compatibility claims in this section are *research starting points*, not approved dependency decisions. Verify licenses, current APIs, security posture and compatibility before selecting a dependency.

## Principles

1. **Macintosh-first, not lowest-common-denominator.** Native OS 9 behavior is the design target. Host-side adapters and fixtures enable tests but don't determine the API.
2. **Modern development, authentic execution.** Develop and cross-build on Linux; verify compiled PowerPC programs on the actual Mac. Clearly distinguish Classic-on-Tiger tests from booted OS 9 tests.
3. **Modern C++ with a small runtime footprint.** Begin with C++17, static linking, bounded memory, explicit ownership and small dependencies. Avoid exceptions/RTTI where unnecessary. Don't impose a large standard library just to make the API look modern.
4. **Cooperative rather than secretly concurrent.** Networking should allow the Toolbox event loop to remain responsive, with defined cancellation and deadlines. Never allow event pumping to start an accidental nested request.
5. **Failures are explicit.** Return typed status/results; specify resource ownership, timeout, partial I/O, close and rollback semantics. Do not silently downgrade TLS, trust checks or entropy policy.
6. **Composable modules, no monolith.** Offline apps shouldn't link networking; networking clients shouldn't need UI widgets; basic utilities shouldn't drag in TLS.
7. **Correctness is demonstrated.** Compile-time checks and host sanitizers complement—but do not replace—target-architecture and actual-Mac tests. Build/test evidence belongs with each feature.
8. **Extract what two applications need.** Resist speculative frameworks. Keep `weather` working while validating public interfaces in a second small native application.
9. **Document old-platform realities.** Resource forks, clocks, memory partitions, import glue, seed provisioning and build-toolchain provenance are part of the supported surface, not implementation trivia.

## Initial target and non-goals

**Target:** Mac OS 9.2.2 on PowerPC, with Retro68's modern GCC cross-compiler and C++17. Release as small, statically linked libraries and headers, with examples and reproducible builds. An OS 9 application must not require a Unix shell or new runtime services on the Macintosh itself.

**Not goals for v0:**
- A general GUI/widget framework, replacement for Clapkit or PowerPlant.
- Another TLS implementation or new cryptography.
- A POSIX compatibility runtime, or the pretense that Toolbox file APIs equal Unix filesystem APIs.
- Binary compatibility with CodeWarrior/Carbon CFM. A future small C ABI is a *separate* interoperability project with its own ABI/ownership tests.
- Equal NetBSD/Linux/Windows platform support or packaging.
- Threading, a general async framework, dynamic plugins or a universal HTTP client before real requirements exist.
- Automatic extraction of the entire `weather` core.

## Candidate module boundaries (not final APIs)

| Module | Candidate responsibilities | Avoid coupling to |
| --- | --- | --- |
| `core` | status/result values, bounded buffers, explicit ownership | UI, networking |
| `time` | monotonic elapsed time, UTC conversion, deadlines, rollover tests | application UI |
| `cooperative` | yield/cancel contract, non-reentrant operation lifecycle | a particular window implementation |
| `open_transport` | endpoint and resolver lifecycle, partial I/O, timeout, teardown | BearSSL or weather-provider parsing |
| `storage` | Preferences and file operations, checked writes, atomicity/failure policy | TLS seed format or weather config schema |
| `tls_adapter` (later) | interface to an existing TLS engine, certificate clock and entropy sources | reimplementation of TLS |
| `toolbox_bridge` (minimal) | optional event pump adapter, no widget toolkit | the rest of the library |

An internal platform-neutral `Io`/clock/status seam may be reused by tests and by `weather` on POSIX; this does not make POSIX a first-class *public release target*.

Potential API properties: no hidden allocations in hot paths; checked buffer capacities; errors that preserve platform codes; explicit state machines; clear initialization/shutdown; callback lifetime rules; no nontrivial work inside interrupt/notifier callbacks; reentrant cancellation prohibited or precisely documented.

## Proposed stages and acceptance

### 0. Study and inventory — no code extraction
- Compare Retro68, Clapkit, macTLS and Certainly from source and licenses.
- Inventory `weather` modules and their hidden globals, event-pump assumptions and application-specific constants.
- Identify a smallest useful shared operation that can be tested independently.
- Write one design note containing the API/lifetime contract and minimal example. No library release yet.

### 1. First extraction
- Start with time/deadline, cancellation and Open Transport resource lifecycle; include small error/result types only as needed.
- Integrate through an in-tree static archive first, keeping `weather` behavior unchanged.
- Run host unit/sanitizer checks, Retro68 compile/link and real PowerPC execution. Add regressions for rollback and shutdown.
- Document where the interface is specifically classic Macintosh rather than portable.

### 2. A second consumer
- Build a very small native example or second app (e.g. a simple network status viewer or RSS proof of concept) without importing weather-specific types.
- Revise the API only when the second consumer exposes a real mismatch.
- Prefer examples exercising normal and failed/cancelled operations.

### 3. Harden and extend deliberately
- Extract checked Preferences/file helpers if both consumers need them.
- Consider an adapter to an existing TLS library after dependency/security review. Keep TLS seed handling and installation secrets separate from distributable application artifacts.
- Benchmark RAM, binary size, event responsiveness and handling on real hardware.

### 4. Publish if proven
- Decide repository/name/license/versioning only after two consumers and a stable boundary exist.
- Publish documented headers/static libraries, a pinned Retro68 build recipe, license notices and simple examples.
- State exactly which OS version, PowerPC ABI, Toolbox/Carbon assumptions and test environment were validated.
- If compatibility with CodeWarrior or 68k becomes desirable, treat each as an independent port with its own compiler and hardware tests.

## Open questions

1. Which Open Transport facilities are generalizable without exposing hidden global single-connection state?
2. What is the smallest safe yield/cancellation contract that integrates with arbitrary Toolbox applications?
3. Can we reuse an existing TLS library, or should the first release intentionally exclude TLS?
4. What is the right boundary for a Preferences/file helper given classic Mac file and resource semantics?
5. Is the future public API C++17-only, with a later optional C facade?
6. What second application provides the strongest test without becoming a second giant project?
7. Which OS 9 and Classic-on-Tiger behaviors must be verified separately?

## Decision requested

Approve **Macintosh-first systems utilities for Retro68/PowerPC** as an exploration, **not** the creation of a new repository or extraction PR today. Keep the four-platform `weather` release plan independent. Proceed with Stage 0 research before altering implementation or promising a library release.
