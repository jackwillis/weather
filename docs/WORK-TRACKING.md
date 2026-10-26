# Work tracking — 2026-10-26

Issues own scope, decisions and acceptance. PRs deliver concrete code or documentation. Closing a planning PR does not complete its proposal.
Start with milestone trackers [v0.1 #45](https://github.com/jackwillis/weather/issues/45),
[v0.2 #46](https://github.com/jackwillis/weather/issues/46), [v0.3 #47](https://github.com/jackwillis/weather/issues/47),
and [Research #48](https://github.com/jackwillis/weather/issues/48).

## Preserved proposals

| Closed planning PR | Current issue | Scope |
|---|---|---|
| #22 | [#96](https://github.com/jackwillis/weather/issues/96) | Release/platform policy; owner decision |
| #27 | [#97](https://github.com/jackwillis/weather/issues/97) | Mac systems library research |
| #33 | [#98](https://github.com/jackwillis/weather/issues/98) | OS 9 UX research |
| #34 | [#99](https://github.com/jackwillis/weather/issues/99) | NetBSD UX research |
| #37 | [#100](https://github.com/jackwillis/weather/issues/100) | Modern C++ research |
| #40 | [#101](https://github.com/jackwillis/weather/issues/101) | v0.2 trust-store proposal |

Each issue preserves the full proposal, original description, source head and discussion links. Original branches remain available.
The closed planning PRs no longer belong to release milestones. Earlier conversions remain #18 → #67, #20 → #68 and #21 → #69.

## Implementation acceptance

Existing implementation PRs now link these specific work issues. Their tests and review evidence remain on the PRs.

| PR | Work issue | Acceptance |
|---|---|---|
| #11 | #102 | Native OS 9 window |
| #12 | #103 | Location and Preferences |
| #15 | #104 | Last-good cache |
| #16 | #105 | NWS request chain |
| #17 | #106 | Place search and first run |
| #19 | #107 | Supplemental Linux/PPC CI |
| #79 | #108 | Open-Meteo units |
| #94 | #109 | Main-branch CI completion |
| #95 | #110 | Labeler configuration |
| #42 | #111 | C ABI/CFM feasibility; PR is only a partial spike |

Use `Closes #N` only when the PR completes that issue; use `Refs #N` for partial work.
No implementation PR was merged or closed during this organization pass.

## Board and execution

The [shared board](https://github.com/users/jackwillis/projects/3) uses Backlog, Ready, In Progress, Review and Done.
Weather issues and implementation PRs are represented; old planning cards are archived. Unrelated repositories are outside this pass.
Hardware and owner labels remain explicit even when independent implementation can proceed. Review status does not confer merge authorization.

Owner decisions #62–#64 and #66 are recorded in their issue bodies with historical triage retained. #49 is unblocked by #62;
#63 still needs Tiger for its runtime experiment, and #65 still needs owner confirmation of licensing wording.

Next implementation is #66: separate production from test-only clock/trust overrides, then verify the TLS policy against every service.
Follow with #49/#62 stale display, #92 manual refresh floor, and #64 recovery guidance. Native acceptance remains gated by Tiger;
no cross-build substitutes for that evidence. The TLS work is unfinished and has not shipped.

The [integration assessment](INTEGRATION-2026-10-26.md) preserves the earlier 33-PR snapshot and interaction risks;
its planning references are superseded by this map. GitHub issues and milestones remain the live source of truth.
