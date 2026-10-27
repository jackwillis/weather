# RFC: Classic Macintosh Networking Runtime and Shared Trust

Status: **Exploratory; no production migration approved** (2026-10-25)

## Product decision

Preserve the ship path for Weather / WhatItBeLike 0.1. Develop a **Macintosh-first static networking SDK and independently maintainable, shared CA trust repository** as a separate track. Evaluate a Code Fragment Manager (CFM) redistributable, but do not make the first Weather release depend upon CFM success. A shared *certificate data store* does not require shared binary code.

The user goal is a classic Mac-native equivalent of a redistributable runtime: multiple applications benefit from independently updated networking code and certificates, using an optional installer. Initial implementation should prioritize the common data format and testable client library, not an Extension or a Control Panel.

## Evidence and limits

- Weather has already run Open Transport DNS/TCP + BearSSL HTTPS with verified certificates in Classic on the real Power Mac G4; its seed lifecycle and I/O cancellation are specific, carefully tested code that should not be destabilized.
- Retro68 can generate PPC PEF applications and includes MakePEF / MakeImport. Its documented `add_application` function emits executable PEF and application resources; it **does not demonstrate a drop-in CFM shared-library target**. A separate build, loader and hardware experiment is required. See https://github.com/autc04/Retro68 and https://github.com/autc04/Retro68/blob/master/cmake/add_application.cmake.
- Apple CFM is the historically appropriate code-sharing mechanism; loading, export/import symbols, `cfrg` resources, architecture and version checks must be verified using Apple's Code Fragment Manager docs: https://developer.apple.com/library/archive/documentation/mac/pdf/PPC_System_Software/Code_Frag_Mgr.pdf
- Alternative stacks exist (Certainly, ClassicNet, macTLS). Compare integration and maintenance before writing a new TLS protocol implementation. Do not promise TLS 1.3 with the present BearSSL path.
- Folder Manager's Application Support location is a candidate for shared certificate data; verify actual folder constants, creation behavior, HFS path handling and ownership on OS 9 before finalizing the installer. Never use Unix-style hard-coded paths on classic Mac.

## Architecture and compatibility contracts

1. **Trust repository**: host-authored collections of per-certificate PEM/DER files, grouped into human-friendly folders, with one active baseline, explicit optional custom additions, and authoritative distrust. A modern-host generator builds a deterministic versioned snapshot; target loaders read the snapshot instead of recursively walking live folders. Use an inspectable manifest and complete snapshot replacement. Invalid explicit snapshots must **not** silently fall back to embedded or previously distrusted roots. Loading must be bounded, fail closed, and preserve hostname/clock checks. The installed trust data is distinct from **secret, rotating** OS 9 TLS entropy seeds.
2. **Native static SDK**: narrow Open Transport DNS/TCP/TLS components and pluggable trust-store loader behind an opaque, versioned **C ABI**; implementation can remain C++17. Explicit callbacks for yield/cancel, allocation ownership and error codes; no exposed STL objects, exceptions, threads, or C++ ABI layout. Do not change Weather's current adapters until a second standalone client is proven.
3. **Optional shared CFM runtime**: same conceptual ABI but proper PEF library packaging, exports, import stubs, `cfrg` and version compatibility. A static link fallback remains supported. Runtime upgrade must not downgrade or replace user trust data. Do not require an OS-level background daemon or code-patching Extension.
4. **User tooling**: a small, documented Python 3 host-side preparation utility with offline inputs and reproducible output; on Macintosh a small native import/inspect app and a manual Finder-copy recovery route. A Control Panel is optional later, not part of MVP.

## Phases and exit tests

### M0 — CFM feasibility, isolated (started in this PR)

- Establish an independent C-only versioned ABI spike in `spike/classic-runtime/` with an exported function-table entry point and a separate client. Prove its semantics with a local C compiler first.
- Next: investigate Retro68 XCOFF/PEF **library** emission, entry points, imports, fragment resources, load search paths and ABI conventions; construct a loadable CFM library and separately built test app on the G4.
- Test missing library, minimum version, compatible update, incompatible update and repeated open/close; record console/test evidence. Do not claim CFM success based on an ordinary application PEF.
- **Exit:** a real Classic PowerPC application loads and calls a separately installed fragment; otherwise document exact blockers and continue static-only.

### M1 — Shared trust data MVP

- Resolve source-of-truth rules and propose a strict versioned format; measure PEM runtime parsing vs host-compiled binary snapshot on OS 9.
- Build a modern-host generator with deterministic sort, X.509 validation, fingerprint-based deduplication, archive/inactive collection handling, full-diff and negative tests. Never treat untrusted arbitrary leaf certs as anchors.
- Add explicit snapshot replacement and safe previous-version recovery. Invalid explicitly selected store must not silently restore old embedded trust. Include revocation/removal semantics, not an additive-only store.
- **Exit:** the *same* Weather binary accepts a newly installed trusted root and rejects a removed root, with malformed/interrupted update tests.

### M2 — Independent static SDK

- Extract the least application-specific Open Transport DNS/TCP, cooperative cancellation, TLS adapter and trust loader behind a stable C API.
- Second tiny application uses the SDK and the same installed trust store.
- **Exit:** independent HTTPS and DNS demonstrations, failure injection, shutdown and memory tests on real G4.

### M3 — Shared CFM redistributable (conditional on M0)

- Build/ship versioned PEF shared fragment and import stub; test two independently linked clients and replacement with a compatible patched fragment.
- Versioned ABI contracts, no cross-fragment C++ ABI ownership, safe upgrade/recovery, no runtime downgrade.
- **Exit:** updating installed runtime fixes a test defect in both unmodified clients.

### M4 — Native installer and certificate management

- Deliver disk image with static/CFM selection as appropriate, optional trust installer, readable diagnostics and a native Certificate Manager *application*. Evaluate Control Panel only if justified.
- **Exit:** clean-install, update, uninstall and recovery walkthrough on a real OS 9 system with no developer tools.

## Sequencing against Weather

**Weather track:** merge critical forecast/TLS correctness fixes, native OS 9 GUI, test on real displays, and release initial packages.

**Runtime track:** M0 can run alongside Weather; M1 is the first useful product feature. M2–M4 should not block Weather 0.1. Coordinate with #27 (Macintosh-first systems library), #40 (external TLS trust), #18 (release), and #33 (OS 9 UX). This RFC is the integration plan rather than permission to rewrite those branches.

## Risks and security decisions

- Classic Mac OS lacks modern local isolation: shared trust data is *centrally maintainable*, not tamper-proof.
- A stale collection must not silently re-add a distrusted root. Source PEM alone does not preserve the complete NSS/Mozilla trust metadata. Importers need explicit documented scope.
- Trust-store file checksum detects corruption, **not** authenticity. Consider publisher provenance and explicit human fingerprint review.
- Must retain certificate name/time checking, no insecure bypass. The shared code should not mix public trust data with seed secrets.
- Updating CA roots cannot fix missing newer TLS algorithms, APIs or protocol changes. No promise of 'HTTPS forever'.
- CFM shared code raises the blast radius of a bad upgrade: static fallback, compatibility check, last-good installer behavior and test coverage are release gates.

## Decision

Proceed with M0 as a tiny throwaway experiment and M1 design in parallel. Choose static SDK as reliable baseline and shared CFM as a *tested optional enhancement*, not a launch dependency.
