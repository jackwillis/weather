# TODO (2026-10-28)

GitHub issues and milestones are the live work list. This file is an entry point,
not a second acceptance checklist. Merged implementation does not imply native
hardware or release acceptance. Start with [v0.1 #45](https://github.com/jackwillis/weather/issues/45).

## Now — v0.1

- [ ] Fix OS 9 window Command-period cancellation (#168) and system-requested quit (#169); add diagnostics, then verify on Classic/native OS 9.
- [ ] Assess the separate Classic/native timing captures and collect the missing restart/repeated evidence (#121), then implement justified first-run entropy collection (#120). The exploratory analyzer credits zero bits; Linux-provisioned seeds remain required today.
- [ ] Finish native window acceptance (#102), MET default fallback/cache acceptance (#152/#128), and DNS/real-display acceptance (#70, PR #14). Some Classic and owner-observed native runs passed; see #45 for their exact scope.
- [ ] Refresh shell/package/delivery safety PR #85 (#57), run combined release-candidate host/sanitizer/32-bit/GUI and NetBSD/macppc checks, and rebuild the package. Jack handles reboots, privileged installation and real-display review.
- [ ] Review test-only PR #154; code/test merges still need Jack.
- [ ] Decide final program names (#135), verify published checksums (#173), and finish provider attribution/terms (#73) and release readiness (#67). Jack tags and publishes.

## Next — v0.2

- [ ] Integrate existing NWS PR #16 (#105) and place-search PR #17 (#106); resolve current-main conflicts and repeat checks. OS 9 search/provider controls also involve #74/#151.
- [ ] Refresh OS 9 transport-lifecycle PR #91 (#60), big-endian CI PR #19 (#107), and changelog-fragment PR #87 (#77). Green historical checks do not validate a conflict resolution.
- [ ] Native OS 9 Preferences (#182, parent #180): app-owned typed storage and controls; remove text Preferences and legacy weather.conf without migration. Coordinate with the current agent owner.
- [ ] Shared presenter (#159), factual headline (#155), entered-location provider policy (#156), digit artwork (#164), Activity Log (#160) and OS 9 log rotation (#161). Follow shared UX #113 and platform plans #98/#99.
- [ ] NetBSD system OpenSSL/trust backend (#158), dependency updates (#172), XQuartz verification (#171), and NetBSD VM/pkgsrc work (#75).
- [ ] Remaining hardening and platform-policy acceptance: use the [v0.2 tracker #46](https://github.com/jackwillis/weather/issues/46).

## Later

- v0.3: hourly/daily data (#117), pinned locations (#118), compact/expanded windows (#176); [tracker #47](https://github.com/jackwillis/weather/issues/47).
- v0.4: CLI revision (#178) and the full documentation pass (#179). Keep current docs accurate meanwhile.
- Research: map feasibility (#114), chart geometry (#174), architecture proposals (#175), conditional reload/daemon (#68), and native Tiger (#69, parked). Research is not a release commitment.

## Implemented milestones

- [x] v0.0.1 pipeline pre-release published 2026-10-27; package install/run/remove and Classic launch evidence are in docs/STATUS.md. This is not the v0.1 release.
- [x] Native OS 9 window and persistent text Preferences (PRs #11/#12 merged); remaining acceptance and the planned native-format replacement are separate.
- [x] Last-good forecast cache (PR #15), MET revalidation/default saving (#146), and automatic Open-Meteo → MET fallback (#163) merged. Target-platform/policy acceptance remains open.
- [x] Fetch deadlines (#41), TLS 1.2 suite policy/test-hook isolation (#145), and diagnostic-only entropy probe (#147) merged. Resolver/native wait limits and production entropy collection remain separate work.
- [x] CLI/X11, managed OS 9 seeds, cross-builds, packaging/man pages and host tests exist. Historical results and reproduction commands are in docs/STATUS.md; repeat acceptance on the combined release candidate.
