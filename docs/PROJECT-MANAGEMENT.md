# How the project is run (2026-10-26)

Weather is a hobby project built by one owner (Jack) with two coding agents, Codex and Claude. This page says where each kind of
thing lives on GitHub so that nobody has to guess. It describes the setup as of the date above; labels, milestones and the board
are the source of truth, not this page.

## Where things live

| Thing | Where | Notes |
| --- | --- | --- |
| A unit of work, bug, finding or question | **Issue** | Use the issue forms: work item, owner decision, hardware acceptance. |
| An implementation | **Pull request** | Closes an issue (`Closes #N`). PRs are for code and for docs that ship. |
| A design proposal | **Issue** labelled `rfc` | Keep the proposal and discussion in the issue; open a PR when implementing an adopted change. |
| What ships when | **Milestones**: `v0.1`, `v0.2`, `v0.3`, `Research` | Percent complete is automatic. No dates. |
| Who is waiting on whom | **Labels** and the **Threshold Backlog project** board | See below. |
| What is verified | PR text, `docs/STATUS.md`, `docs/verification/` | Say plainly what ran on hardware, in a simulator, or not at all. |

Issues and PRs share one number space, so `#12` may be either.

## Milestones

Each milestone has a tracking issue linking its work: **#45** (v0.1), **#46** (v0.2), **#47** (v0.3), **#48** (Research). Start there to see what is left.

* **v0.1**: first usable NetBSD and Mac OS 9 release. An ordinary user can install, configure and use every platform we advertise.
  Keep it narrow; the tracking issue lists the gate.
* **v0.2**: resilience and preservation (offline cache, more sources, location search, hardening).
* **v0.3**: platform integration and SDK (provisional).
* **Research**: RFCs and feasibility studies with no release commitment (for example native Tiger).

Every open issue and PR should be in exactly one milestone. A plan-only PR with no code should be an issue instead, with the plan text as its body.

## Labels

* **Area**: `area/core`, `area/os9`, `area/netbsd`, `area/tls`, `area/ci`, `area/docs`. PRs get these automatically from the files they change
  (`.github/labeler.yml`); issues are labelled by hand.
* **Kind**: `bug`, `enhancement`, `audit-finding`, `rfc`, `decision`, `documentation`.
* **Waiting on**: `needs-owner` (a decision only Jack can make), `needs-hardware-tiger` (the Mac must be booted into Tiger/Classic),
  `needs-hardware-netbsd` (the Mac must be booted into NetBSD), `blocked` (name the blocker in a comment).
* **Who**: `agent/codex`, `agent/claude`. Set it when you start, remove it when you stop.

## The board

The **Threshold Backlog** project (https://github.com/users/jackwillis/projects/3) holds this repository's issues and PRs next to the owner's
other work. Its Status field is **Backlog, Ready, In Progress, Review, Done**. Filter by repository and milestone for the weather items.
Backlog holds unadopted proposals and deferred work; Ready holds actionable work; In Progress means implementation or acceptance work is active;
Review means an implementation awaits review or merge authorization. Done requires completed acceptance, not merely a closed planning PR. Hardware-gated work is easiest to
see with the filter `label:needs-hardware-tiger` or `label:needs-hardware-netbsd`: the owner boots the Mac into one OS and the agents clear that
whole list in one sitting.

## Merging

Merge commits are used, not squash: stacked PRs (a child branch based on its parent) would break under squash. Use `Closes #N` in the PR
text so merging closes the issue. Delete the branch after it merges.

## Pull request flow

1. Open the PR early as a draft. Link the issue. Keep it to one purpose.
2. Stacked PRs say so in the first line ("Stacked on #N") and set their base to the parent branch. Merge parents first, then retarget the child to `main`.
3. Before it is ready: `make check` and `make check-san` pass, hosted CI is green, and the PR text separates hardware results from simulated ones.
4. A review comes from the other agent or from Jack. Reviews state the head SHA they cover, and say what was run and what was only read.
5. **The owner decides what merges.** Agents do not merge unless Jack has said so for that batch. When several PRs are merged together, each is
   merged on top of the previous result and the full checks are re-run on the combination.
6. Until #77 (changelog fragments) lands, each PR adds a line to `CHANGELOG.md`. Expect conflicts there when several PRs merge together; keep both entries.

## Automated checks

* `checks.yml`: host, GUI, sanitizer and 32-bit checks on every PR and push to `main`.
* `policy.yml`: `tools/check-policy.sh` fails the build if a seed file or import, an Apple copyright notice, or a classic-Mac archive is tracked.
  These are standing rules from `AGENTS.md`.
* `labeler.yml`: applies `area/*` labels to PRs from the paths they change.

None of these replace a review. They cover the rules that a machine can check, so reviewers can spend their time on design and risk.

## Working with the agents

* Codex leads implementation; Claude reviews, researches and does triage. The roles can change for a session when Jack says so.
* Coordination notes between the agents are plain files outside the repository (`~/scratch/memo-*.md`). Anything that matters long term belongs
  in an issue or in `docs/`.
* Hardware access (the Power Mac G4, "maul") is through `maulctl`; see `AGENTS.md`. Agents never compile on maul, never use root, and never answer system dialogs.

## Where things were converted from (2026-10-26)

The first triage turned plan-only draft PRs into issues: #18 became #67, #20 became #68, #21 became #69 (the plan files stay on their closed branches).
The audit in `docs/AUDIT-2026-10-25.md` was split into issues #49 to #66 by theme; finding IDs (A1, E3, ...) are in the titles. Old issues #3 to #6 were closed
as done or superseded. The subsequent organization pass moved planning-only #22, #27, #33, #34, #37 and #40 into issues #96–#101.
Their PRs are closed without merging; proposal text, discussion links and branches are preserved. Concrete spikes remain PRs.
See [the work map](WORK-TRACKING.md) for implementation acceptance issues and board conventions.
