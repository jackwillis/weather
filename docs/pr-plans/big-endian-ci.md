# Big-endian CI

Status: draft feasibility/design PR; no runner or emulator has been selected.

Scope: complement #10's 32-bit x86 checks with reproducible big-endian execution.
Evaluate a suitable emulated NetBSD/macppc test environment and toolchain; document
cost, startup time, memory, provenance and failure diagnostics before selecting it.

Acceptance:
- Pin/download only redistributable toolchain and OS artifacts with integrity
  checks. No Apple images/SDKs or personal machine images in CI.
- Execute unit and mutation binaries on a genuine big-endian runtime and prove
  expected word size/endianness. A cross-compile alone does not satisfy this gate.
- Distinguish NetBSD syscall coverage from an optional Linux/PPC emulator check;
  neither silently replaces real maul smoke/GUI acceptance.
- Demonstrate a seeded endian/overflow regression fails the gate and remove it
  afterward. Keep runtime/resource limits practical and preserve useful logs.
- Never put maul SSH keys or private TLS seeds in hosted CI. A self-hosted/LAN
  alternative needs a separate security/operations decision from Jack.

Dependencies: CI PR #10; hardware acceptance remains independent ground truth.
