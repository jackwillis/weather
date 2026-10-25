# Native Mac OS X Tiger feasibility

Status: parked, research-only roadmap draft. NetBSD and Mac OS 9 remain the active
targets. Opening this PR does not unpark a native Tiger implementation; Jack must
explicitly decide to do that after reviewing bounded feasibility findings.

If activated, evaluate a modern Linux cross-toolchain capable of the C++17 core
with the old Darwin/PowerPC ABI, then a minimal hello/network/TLS probe. Preserve
the portable layers and OS seams; do not rewrite the core into C++98 as a shortcut.

Acceptance for research:
- Record actual compiler/linker/ABI availability, reproducible build steps and
  licensing constraints. Use only the owner's legitimately supplied SDK outside
  the repository; never copy Apple headers/binaries into source or CI artifacts.
- Cross-build on Linux, never maul. Separate compiler/link success from verified
  execution on Tiger. Ask for any required boot/login; do not operate system dialogs.
- Verify clock/entropy/TLS assumptions independently; never reuse or export the
  OS 9 installation seed for another platform.
- Produce a go/no-go recommendation with effort and test requirements. A native
  UI/full port belongs in a newly approved implementation scope after that decision.

Dependencies: none for active OS 9 work; this PR is deliberately non-blocking.
