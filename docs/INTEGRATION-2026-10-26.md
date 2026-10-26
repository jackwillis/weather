# Weather integration assessment — 2026-10-26

Snapshot: origin/main `ebeaaed8c70d58b70a660069ab10dba022fd1a8c` (includes project-management PR78). Main Checks and Policy passed. Working tree was clean before this documentation update. This is a portfolio/integration assessment of all open PR descriptions, reviews, changed files, code interactions and CI; it is not a new independent line-by-line approval or a rerun of every branch suite.

33 open PRs after adding the immediate labeler fix95. No PR merged by this assessment.

## Decision and execution order

1. Repair the shared label check with95 and review94 so sequential main commits retain completed CI. The existing red label jobs fail with `found unexpected type for label area/core`; product checks and policy are separate and passed on41/93.95 preserves all six labels/path globs and the workflow security boundary. Because labeler uses pull_request_target, its real fix cannot run from a PR head before main pickup.
2. Review a small integration batch:87,80,79,81,82,84,89. Ready/draft flags and green CI do not substitute for another reviewer. Integrate only after Jack authorizes the batch, then run complete checks on each combined result. No issue is closed until all of its acceptance is actually met.
3. Highest new implementation priority: decided66, beginning with production/test build separation for WEATHER_NOW and WEATHER_NO_TA, followed by TLS1.2/explicit suites and live compatibility checks of every service. Keep the current certificate/clock negative tests using explicitly test-built binaries; production must ignore the variables. This audit does not change TLS behavior.
4. Implement newly unblocked49/62: age first, dim stale values; then92 five-second manual refresh floor in Model and both windows, with automatic refresh and CLI unaffected. Add model and GUI regressions. Implement64 lock-only recovery documentation and matching About guidance.
5. Resolve11 and43 conflicts against current main, reconcile41/91 shared clock/lifecycle/layout changes, then integrate the OS9 stack in the approved order11 ->12;15 branches from11,17 follows12. Do not claim merged-tree or native runtime acceptance from separate branch test totals.
6. Keep15/16/17 and trust-store40 in v0.2; keep27/37/42 and broader compatibility experiments in Research. v0.3 remains intentionally empty until research is adopted. No additional feature PRs just to fill the roadmap.

## Milestones and board

The real milestones exist: v0.1 (first usable NetBSD/macppc and OS9 release), v0.2 (resilience/preservation), v0.3 (provisional), Research. Tracking issues45–48 still contain obsolete statements that milestones do not exist, closed PR references18/20/21, and decision-waiting lists superseded by owner comments. Follow actual milestone assignments and owner comments; refresh tracker prose rather than reopening decided questions.

Board: Threshold Backlog, project3. The exported weather items have no populated Status field, so their presence alone is not progress tracking. Record Todo/In Progress/Done consistently in a later board-maintenance pass, limited to weather items. This audit did not change unrelated board items.

Metadata corrected:79 ->v0.1;80/81 ->v0.2 to match their linked issues.87 retargeted from merged78 to main.95 assigned v0.1. No milestone scope expansion or PR merge.

## Every open PR

| PR | Milestone | State at snapshot | Decision |
|---|---|---|---|
| [#11](https://github.com/jackwillis/weather/pull/11) Add the first native Mac OS 9 weather window | v0.1 | draft; conflict | Resolve main conflict and review integration with41/91/43; Tiger background/menu/production delivery pending. |
| [#12](https://github.com/jackwillis/weather/pull/12) Add OS 9 location dialog and persistent preferences | v0.1 | draft | Parent11; native Preferences/dialog/restart acceptance pending. |
| [#14](https://github.com/jackwillis/weather/pull/14) Complete DNS and NetBSD hardware acceptance | v0.1 | draft | Hardware gate; real-display owner review and deliberately stalled/delayed DNS remain. |
| [#15](https://github.com/jackwillis/weather/pull/15) Persist and display the last good forecast offline | v0.2 | draft | v0.2; parent11; merge model/cache semantics carefully; native persistence acceptance pending. |
| [#16](https://github.com/jackwillis/weather/pull/16) Add multi-step NWS weather provider support | v0.2 | draft | v0.2; host/G4 NWS chain exists; preserve multi-step budgets and cache source identity. |
| [#17](https://github.com/jackwillis/weather/pull/17) Add location-name search and first-run selection | v0.2 | draft | v0.2; parent12; native search/first-run runtime acceptance and provider choice74 pending. |
| [#19](https://github.com/jackwillis/weather/pull/19) Add reproducible big-endian validation to CI | v0.1 | draft | Review supplemental Linux/PPC QEMU gate; retain94 concurrency change; not NetBSD acceptance. |
| [#22](https://github.com/jackwillis/weather/pull/22) Document initial release matrix and five-tier platform policy | v0.1 | draft | Owner scope proposal; do not advertise extra targets before release acceptance. |
| [#27](https://github.com/jackwillis/weather/pull/27) RFC: A Macintosh-first modern systems library for Retro68 | Research | draft | Research only; no library extraction before second consumer/evidence. |
| [#33](https://github.com/jackwillis/weather/pull/33) RFC: Classic Mac OS 9 error UX, Activity Log, and native affordances | v0.2 | draft | Design reference; no Activity Log implementation claimed. |
| [#34](https://github.com/jackwillis/weather/pull/34) RFC: NetBSD/macppc X11 and ctwm UX design | v0.1 | draft | Design reference; implement selected decided items49/92, not an entire UX framework. |
| [#37](https://github.com/jackwillis/weather/pull/37) RFC: Evidence-driven modern C++20/23 support and adoption for Retro68 | Research | draft | Research only; preserve C++17 product baseline. |
| [#40](https://github.com/jackwillis/weather/pull/40) RFC: Long-lived user-maintained TLS trust stores on NetBSD and Mac OS 9 | v0.2 | draft | Deferred v0.2 RFC: trust snapshots/distrust are not implemented. |
| [#41](https://github.com/jackwillis/weather/pull/41) Bound fetch I/O and share POSIX connection timeouts | v0.1 | draft | Review current monotonic/deadline head; blocking POSIX DNS and OS9 TCP/stream inner budgets remain. |
| [#42](https://github.com/jackwillis/weather/pull/42) RFC + initial C ABI spike: classic Macintosh networking redistributable | Research | draft | Research: C ABI host spike is not a working shared CFM runtime. |
| [#43](https://github.com/jackwillis/weather/pull/43) Audit Open Transport glue provenance, documented context APIs, and OS compatibility | Research | draft; conflict | Resolve main conflict; overlap with91 is intentional native guards.63 runtime constant experiment awaits Tiger. |
| [#44](https://github.com/jackwillis/weather/pull/44) Docs: align README, roadmap, status and architecture with active PRs | v0.1 | draft | Refresh this portfolio/status view as branches merge; documentation is not verification. |
| [#79](https://github.com/jackwillis/weather/pull/79) Reject malformed explicit Open-Meteo unit metadata | v0.1 | ready | Ready for independent review/integration: explicit unit metadata. |
| [#80](https://github.com/jackwillis/weather/pull/80) Allocate private fixture ports and GUI displays | v0.2 | ready | Ready for independent review/integration: fixture isolation; all GUI runs stay serial. |
| [#81](https://github.com/jackwillis/weather/pull/81) Harden HTTP framing and abrupt TLS closure handling | v0.2 | ready | Ready for independent review/integration: HTTP framing and authenticated TLS EOF. |
| [#82](https://github.com/jackwillis/weather/pull/82) Preserve automatic refresh deadlines across option toggles | v0.1 | draft | Review refresh scheduling fix;51 observation-time validation remains separate. |
| [#83](https://github.com/jackwillis/weather/pull/83) Preserve source errors and emit complete traffic dumps | v0.2 | draft | Review error/dump semantics; preserve alongside41 and81;53 DNS gap remains. |
| [#84](https://github.com/jackwillis/weather/pull/84) Prevent Location button replay and guard empty text rendering | v0.1 | draft | Review button replay/CDE fix;55 still has dialog scheduling and Unicode gaps. |
| [#85](https://github.com/jackwillis/weather/pull/85) Harden session edits, package transfers and OS 9 delivery status | v0.1 | draft | Review script safety; native Tiger delivery/resource-fork acceptance pending. |
| [#86](https://github.com/jackwillis/weather/pull/86) Reject zero-filled TLS seeds and test seed continuity | v0.1 | draft | Review zero-seed/continuity slice;59 still admits the known nonzero fixture. |
| [#87](https://github.com/jackwillis/weather/pull/87) Use independent changelog fragments for release notes | v0.1 | draft | Parent78 merged; now targets main. Review fragment tool, then adopt for later integrations. |
| [#88](https://github.com/jackwillis/weather/pull/88) Refuse unsafe settings file replacement | v0.2 | draft | Review safe file refusal;54 still has ACL/group/path-race/directory-durability gaps. |
| [#89](https://github.com/jackwillis/weather/pull/89) Recognize published MET thunder-shower spellings | v0.1 | draft | Review MET spelling fix;50 remains open for sleet/unknown-symbol policy. |
| [#90](https://github.com/jackwillis/weather/pull/90) Keep weather artwork and Location fields within bounds | v0.2 | draft | Review X11 field/art/duration fixes; retain duplicate CDE guard from84/17; native art acceptance pending. |
| [#91](https://github.com/jackwillis/weather/pull/91) Preserve OS 9 DNS results and close endpoints before transport shutdown | v0.2 | draft | Review lifecycle, cleanup and target-layout tests; integrate with41/11; native OS9 acceptance pending. |
| [#93](https://github.com/jackwillis/weather/pull/93) THIRD_PARTY: draft notice for Retro68 runtime and Multiversal Interfaces (#65) | v0.1 | draft | Owner licensing wording/import-library decision65; do not equate a notice with clearance. |
| [#94](https://github.com/jackwillis/weather/pull/94) CI: let every commit on main finish its checks | unassigned | ready | First review batch: preserve per-commit main CI; keep PR supersession. Preserve when integrating19. |
| [#95](https://github.com/jackwillis/weather/pull/95) Fix shared PR labeler configuration schema | v0.1 | draft | First review batch: shared labeler schema fix; live label validation requires main pickup. |

## Verification and owner gates

- No new product tests were run for this assessment; existing evidence files and current CI were read.95 received YAML/schema/path-preservation validation and repository policy validation only.
-41 and93 red label jobs share the main-configuration failure.43 has no current check run in this snapshot and conflicts; absence of a check is not a pass.44 had an old canceled host run; refresh documentation and let its new head run.
- Only11 and43 were reported conflicting in the refreshed snapshot. Unknown/transient merge states must be rechecked before integration.
- Hardware still required: OS9 menu/background/Preferences/search/cache/lifecycle acceptance while Tiger is parked; real NetBSD display judgment; owner package install. All cross-compilation stays on Linux.
-65/93 licensing/import-library wording still needs Jack confirmation.63 requires the approved wording work and a native experiment before removing the unexplained constant; a compile or fake OT test cannot prove it unnecessary.
- Known remaining implementation gaps include POSIX interruptible DNS, OS9 inner TCP/I/O budgets, provider observation-time age validation, Unicode input/dialog scheduling, nonzero fixture-seed admission and settings metadata/durability. They are not all resolved by the current draft fixes.
- Deferred research is not a release blocker unless Jack explicitly adopts it. Native Tiger remains parked.

Labeler schema reference: [actions/labeler documentation at the pinned commit](https://github.com/actions/labeler/tree/bf12e9b00b37c5c0ca2b87b79b2daf7891dbda13#usage). The fix uses its changed-files match-object form.
