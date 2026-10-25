# RFC: User-maintained TLS trust stores for long-lived Macintosh and NetBSD releases

**Status:** Proposal; no implementation or verified compatibility yet.  
**Date:** 2026-10-25  
**Motivation:** Let users keep TLS certificate-authority trust current even if the application is no longer maintained or its binaries cannot be rebuilt.

## Manifesto

A working program should not become unusable merely because its original author stopped publishing binaries and a server's certificate chain moved to a new root. On old hardware, trust material must be **data that can be reviewed, updated and installed separately from the executable**, with tooling that runs on a modern computer and an intentionally small runtime loader.

But a trust store is a security policy, not just a bag of certificates. **Additive-only trust is unsafe**: old CAs can be compromised, distrusted or retired. Updates must support **replacement, explicit custom additions and distrust/removal**. No mode may silently turn off certificate or hostname verification.

## Existing architecture

Currently `src/tls/roots.pem` has three roots (ISRG X1/X2, HARICA TLS RSA Root CA 2021); `tools/gen-anchors.sh` runs BearSSL's `brssl ta` and emits `src/tls/anchors.h`; `src/tls/tls.cpp` passes static `TAs` and `TAs_NUM` to `br_ssl_client_init_full`. Both POSIX and OS 9 application variants use this TLS code. Updating a root currently means regenerating and redistributing binaries.

BearSSL documents both compile-time `brssl ta` generation and runtime PEM/X.509 decoding (`br_pem_decoder_context`, `br_x509_decoder_context`): https://bearssl.org/api1.html .

**Do not reuse, combine with or overwrite OS 9's managed TLS entropy seed.** `docs/OS9-SEEDS.md` describes secret, exclusive, rotating seed state; public CA certificates are nonsecret, independently versioned data.

## Proposed external interface (design to validate)

User-visible modes should be clearly different:

- **Default (no external store):** use embedded compiled anchors as today, with a visible status indicating embedded fallback.
- **Replace:** use the installed, explicitly selected external trust snapshot, *without silently retaining old embedded roots*. This is the preferred mode for routine CA-store refresh and distrust.
- **Add:** user explicitly opts to trust one or more extra roots (for example, an organization's CA). Provide name, SHA-256 fingerprint, source and warning that this broadens trust. Mere presence of a file must not silently elevate it.
- **Distrust:** user can mark specific certificate/SPKI fingerprints as disallowed. Apply exclusions consistently, including to embedded roots if an opt-in union mode is ever introduced. Avoid pretending removal from a generated PEM is adequate when embedded fallback can re-add it.
- **Recovery:** invalid/corrupt/newer-version external stores must produce a clear diagnostic; do **not** silently fall back to embedded anchors when the user selected a replacement or distrust policy. Preserve the previous valid installed version during atomic updates.

Open decision: whether v1 should implement **replace only plus a separate curated custom-root list**, deferring a flexible union/distrust engine. This is preferable to an overengineered first version if it can safely meet the revocation requirement.

## Trust source and generator

Prefer a **small portable Python 3 script using standard-library parsing and file I/O**, runnable on Linux, macOS and Windows. Online fetch should use the host's verified TLS stack and a documented official source. It must also accept a locally acquired PEM snapshot for fully offline operation; no Python or network dependency exists on the vintage target.

Candidate sources:
- NetBSD/pkgsrc `security/mozilla-rootcerts`, including its packaged Mozilla CA bundle: https://cdn.netbsd.org/pub/pkgsrc/current/pkgsrc/security/mozilla-rootcerts/
- curl's Mozilla-derived PEM export: https://curl.se/docs/caextract.html
- Mozilla's NSS `certdata.txt` trust store (trust flags/constraints) for stronger policy fidelity: https://firefox-source-docs.mozilla.org/security/nss/runbooks/rootstore.html

**Trust-source caution:** a PEM of Mozilla roots alone does **not** carry every Firefox/NSS trust restriction, disallowed status, name constraint or policy update. The curl export explicitly warns of missing restrictions. Do not describe importing all PEM roots as exact parity with browser policy; document the selected snapshot and known limitations. Source download integrity/authenticity must be anchored in the modern host's HTTPS/TLS or independently pinned/checksummed metadata; a SHA-256 printed by the same compromised download channel is not an independent authenticity guarantee.

Generator should:
- verify all input PEM/DER structure and relevant CA/public-key rules (do not treat arbitrary leaf certs as unrestricted trust anchors);
- list subject, SHA-256 fingerprint, validity dates, source, generation date, trust-policy revision where available;
- reject duplicates, unexpected private keys, impossible counts/sizes, malformed ASN.1, unsupported algorithms and oversized input; record skipped entries with reasons;
- provide `inspect`/`diff`/`build` operations and a clear **added/removed** summary before installation;
- never generate or include private TLS entropy seed material;
- fail closed on ambiguous policy, parse errors, unsupported bundle versions and incomplete writes;
- operate offline with `--from-pem`, and make any online mode explicit and opt-in.

A dependency-free tool is preferable, but do not implement fragile custom ASN.1 or certificate-policy parsing merely to avoid a well-established dependency. Evaluate whether a verified existing utility such as BearSSL `brssl` can serve as a build-time converter. The v1 executable may be accompanied by a small signed/checksummed package with a readable manifest; these choices need a prototype.

## On-disk representation (evaluate before finalizing)

Do **not** assume `brssl ta`'s generated C header is a suitable portable external format. It is compiled source, not an architecture-independent data file.

Candidate formats:
1. **PEM bundle + optional policy manifest**: easiest to inspect/export, but introduces runtime PEM/X.509 parse cost on 32-bit OS 9 and must implement trust metadata. Potentially 200 KB+ and ~100 anchors.
2. **Versioned canonical binary anchor bundle** with fixed-width, specified endianness fields, lengths, count, CRC/checksum and a separate manifest. Generated on a modern host, bounded loader on target; smaller/runtime efficient, but the spec/parser must be carefully fuzzed and independently tested.
3. **Individual DER PEM roots in a directory**: user-friendly on Unix, but multiple-files and classic Mac resource/file semantics complicate installation, ordering, partial updates and deduplication.

Provisional preference: **one versioned external trust snapshot** plus readable metadata, produced on a host and atomically installed. Decide PEM vs portable binary only after measuring on OS 9. Checksums catch corruption; they **do not authenticate** an untrusted CA list. An attacker able to replace a trusted local bundle may be able to compromise HTTPS, so file ownership/permissions and provenance matter.

Runtime must validate magic/version, lengths, count, caps and public-key material before adopting any new store; load or switch **atomically as a complete valid set**. Keep it in bounded storage for the lifetime required by BearSSL's `br_x509_trust_anchor` pointer fields. Avoid unchecked allocations or pointers into temporary decoded buffers. Record memory and startup costs on the actual G4.

## Platform idioms

### NetBSD/macppc (and NetBSD/amd64)

- First explore consuming the existing NetBSD/pkgsrc CA infrastructure and `certctl`/Mozilla roots rather than maintaining a second automatic download ecosystem.
- A **per-user override** could live under `~/.config/belike/trust/` or another explicitly documented configuration directory. If supporting XDG directories, implement explicit fallback behavior appropriate to BSD; do not assume every desktop has XDG machinery.
- A package-managed default may live under a conventional `/usr/pkg/etc` path. Don't edit a package-owned file or OS-wide CA store on behalf of a regular user.
- Document precedence among command-line/config, per-user override, system store and embedded defaults. Permissions should prevent untrusted users from modifying system roots.
- Do not silently interpret unrelated OpenSSL trust files as an exact BearSSL policy equivalent.

### Classic Mac OS 9 / PowerPC

- Use `FindFolder` for the Preferences folder and a distinct app-owned trust area (provisional name **WhatItBeLike Trust**, subject to branding decision). Keep the existing entropy-seed files separate.
- Bundle an **optional installer/importer** or clearly documented manual Finder-copy workflow. Prefer a single transfer artifact to avoid fragile multi-file updates. Never require Python or a shell on OS 9.
- An installer should validate the entire snapshot before checked replacement and report clear errors; copy/flush/exchange semantics must be verified on actual OS 9 filesystems. Do not overwrite the last good trust file on partial failure.
- If target-specific file metadata (type/creator/resource fork) is needed, set it in the installer; the certificate payload should remain plain data.
- Provide a native UI indication such as **Certificate Authorities…** or **Trust Store Information…**, showing source/date/count and whether embedded or external. Direct arbitrary CA selection inside the app can come later.

## Threat model, lifetime and unavoidable limits

- This mechanism **does not guarantee forever HTTPS**. Server protocol/cipher evolution beyond BearSSL's supported TLS profile, certificate algorithms, clock validity, DNS changes and API retirement can still break the client.
- A newer trust snapshot is not inherently safer if it lacks source authentication or mistakenly adds malicious roots.
- Removal/distrust must work. Current NetBSD 2026-10-21 CA-compromise advisory illustrates why a permanent additive-only collection is unsafe: https://blog.netbsd.org/tnf/entry/emudhra_tls_ssl_ca_compromise
- Clock validation and server-name verification stay enabled. Never suggest accepting expired or unknown issuer certificates as a maintenance shortcut.
- A compromise of installation media or trust-file directory is a serious trust-root compromise. Restrict system-store writes, surface policy changes and fingerprint diffs.
- Embedded fallback after a user-requested replacement can silently resurrect a distrusted CA; this must be explicitly prevented.

## Proposed phases

### Phase 0 — Design + feasibility; no product behavior change
1. Inventory `br_x509_trust_anchor` allocation/ownership and BearSSL parsing; profile footprint on OS 9.
2. Define trustworthy input sources, metadata/constraints and a release-independent versioned format. Compare PEM against compact binary under 32-bit big-endian PowerPC.
3. Define install-location precedence and fail-closed policy, including replace vs custom-add/distrust behavior.
4. Prototype generator with fixtures, malformed inputs, duplicate/distrust tests, and deterministic builds.

### Phase 1 — Safe replace mode
1. Add `belike-trust` or other agreed host-side generator with `inspect`, `diff`, `build` and offline `--from-pem`.
2. Implement tightly bounded runtime loader, valid-before-switch semantics, clear errors and visible provenance.
3. Add platform-native NetBSD user install and OS 9 copy/import workflow.
4. Keep embedded fallback **only if no external store/policy is configured**.

### Phase 2 — Optional explicitly approved custom roots and revocations
1. Introduce explicit custom trust additions and per-root removal/distrust policy only after semantics are tested; support rollback and audit.
2. Never auto-merge stale embedded anchors into newly refreshed external snapshots.
3. Offer simple CLI/UI inspection and diagnostic logs without exposing secrets.

### Phase 3 — Release hardening
1. Test trust rotation (including removed CA), corrupt/truncated/wrong-version/oversized stores, symlinks/path ownership, rollback, power-loss failure injection and partial install.
2. Test on native NetBSD/macppc and real PowerPC Classic/OS 9; assess disk, memory, binary-size and startup overhead.
3. Publish long-term archival documentation: where to obtain trusted CA updates, how to regenerate from local data, how to inspect fingerprints, how to recover from an invalid file, and what TLS limitations remain.
4. Coordinate with package/release readiness (#18), OS 9 installer/UX (#33), and platform policy (#22). Do not mix this with secret seed installation.

## Acceptance criteria

- Users can change CA trust on both primary platforms **without recompiling**.
- New roots can be trusted explicitly; previously trusted roots can be removed or distrusted; the effective trust set is inspectable.
- A configured external trust store **never silently falls back** to stale compiled roots on parse/read failures.
- All certificate verification, certificate-date and hostname checks remain enabled.
- No downloading or modern interpreter is required *on* OS 9.
- A complete, valid previous bundle survives interrupted updates.
- Offline fixture-based tests cover input validation, canonical output, loader bounds, big-endian operation, CA changes and denial of malicious/corrupt stores.
- Distribution docs do not promise indefinite TLS compatibility or accurate replication of full Mozilla/NSS trust policy when using PEM-only exports.

## Decision requested

Approve an **exploratory RFC** for an externally maintainable, replacement-capable CA store and a modern-host conversion tool. No runtime changes, installer commitments or privileged operations are authorized by approving this document.
