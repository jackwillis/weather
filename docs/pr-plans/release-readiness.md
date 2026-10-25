# Release readiness

Status: draft release checklist, not a claim that a release is ready.

Scope: update stale status/roadmap statements, verify provider attribution/terms,
prepare pkgsrc source packaging and refreshed NetBSD binaries, and verify
reproducible release artifacts. Review optional xweather app-defaults support if
real-display feedback establishes a need.

Acceptance:
- Check current official Open-Meteo/MET Norway policies and any subsequently added
  providers; implement required attribution and document distribution assumptions.
- pkgsrc Makefile/DESCR/PLIST/distinfo checked in a suitable NetBSD VM; never build
  on maul. Confirm install/uninstall, man pages, licenses and dependencies.
- Rebuild binary packages; Jack performs privileged reinstall through a reviewed,
  idempotent script dry-run against a copy. Read the resulting log; no root bypass.
- Run clean committed-tree dist/distcheck, compare reproducible archives, inspect
  package contents and confirm budgets/debug symbols as applicable.
- Exclude private seeds, imports, Apple SDK material and local credentials from
  all artifacts. Installer/app updates must preserve per-installation seed state.
- Replace obsolete claims that GitHub does not exist; distinguish historical
  evidence from current results and unresolved hardware checks.

Dependencies: target release features (#11/#12 for the OS 9 UI) and hardware
acceptance. No tag, release publication or extra repository creation is authorized
by this planning PR alone; those remain an explicit owner decision.
