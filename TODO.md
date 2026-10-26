# TODO

Current work is tracked in real GitHub milestones and issues45–48. See [the current issue and board map](docs/WORK-TRACKING.md) and [the complete PR and integration assessment](docs/INTEGRATION-2026-10-26.md); the checklist below is execution order, not a duplicate completion database.

## Next implementation and review

- [ ] Review shared CI labeler fix95 and main-run concurrency94.
- [ ] Review/integrate87,80,79,81,82,84,89 only after owner-authorized merge batch; rerun combined checks.
- [ ] Implement decided66: production ignores test clock/anchor environment overrides; explicit test builds retain negative tests. TLS1.2/modern suites require live compatibility checks for all services.
- [ ] Implement decided49/62 stale-age-first/dimmed display and92 five-second manual refresh floor; automatic refresh/CLI unaffected.
- [ ] Update lock-only recovery64 and About guidance; keep tools for later.
- [ ] Resolve11/43 conflicts and reconcile41/91 before native stack acceptance. Follow11 ->12;15 branches from11 and17 follows12.
- [ ] Complete remaining DNS, timestamp, input, seed-policy and settings gaps identified in the assessment.

## Owner/hardware/release gates

- [ ] Jack confirms licensing/import-library wording65/93 and release target scope.
- [ ] Native OS9 acceptance remains pending while Tiger is parked; do not reboot or claim cross-builds as runtime evidence.
- [ ] Owner reviews xweather on the physical NetBSD display and performs privileged package installation.
- [ ] Final attribution, install/recovery, clean release artifacts and full combined-tree checks before v0.1.

## Later

- [ ] v0.2 cache15, NWS16, search17 and trust-store issue101 according to their acceptance gates.
- [ ] Research issues97/100/111 and PR43 stays separate. Native Tiger69 and conditional reload68 remain parked.

