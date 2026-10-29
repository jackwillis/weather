# OS 9 About / Help copy for v0.1 (#199)

Draft, 2026-10-29. Copy proposal only; not wired into native resources or bundled
Read Me yet. Do not ship the placeholder page. Final wording depends on #120;
page layout, discoverability and screenshots require #204 acceptance.

## Current surface audit

`src/ui/os9/window.cpp` currently has About, shared credit pages and Seed recovery.
Return advances; Escape closes. Basic location/units/offline instructions are not
a dedicated help page. `src/app/credits.hpp` supplies provider/software notices;
#73 owns their correction, including NWS and the CC BY address. `tools/stage-dsk.sh`
bundles LICENSE, THIRD_PARTY.md and a short Read Me; it does not yet bundle this
proposal. Preserve those full notices instead of copying them into every page.

Proposed navigation: keep About in the Apple menu and add a clearly labelled
“Using Weather” route to the same compact page sequence. Use existing window
primitives; this proposal does not add a widget framework or promise a new menu.
Keep body lines short and verify the actual font/width before implementing.

## About

What It Be Like? — Weather 0.1.0

Weather for your classic Macintosh.
Copyright (c) 2026 Jack Willis.
Free software: GPL version 3 or later.
There is no warranty.

Read Me: getting started and known limits.
Credits: data sources and software.
Full notices: LICENSE and THIRD_PARTY.md.

## Using Weather

Choose Location to change your place.
Find searches only when you ask it to.
Choose a result, then save the location.
Options changes units and auto-refresh.
Refresh asks for new weather now.
Wait five seconds between refreshes.

If an update fails, old weather stays.
Its age comes first; old values fade.
Check the age before using the values.

## Saved weather

Save forecasts keeps your last weather
for the next launch or a network outage.
Saved weather may be out of date.
A failed update keeps the old values.
A successful update replaces them.

Turning saving off removes saved weather.
MET Norway needs saving enabled.
Open-Meteo works with saving off.

## First-run TLS setup — PLACEHOLDER, DO NOT SHIP

[Replace after #120's reviewed design is final. Explain the actual user action,
progress, cancellation, failure and retry in plain language. State the accepted
policy's limits; do not describe mouse movement or a completed progress bar as
proof of secure randomness. Do not invent a setup command, menu, collector or
recovery button. Current builds still require separately provisioned managed
seeds; existing instructions are in OS9-SEEDS.md. Claude owns this section's
technical dependency, #193/#120.]

## Clock and connection problems

Check your Mac's date, time and time zone.
Check the network, then try Refresh.
Old weather stays visible after failure.

A certificate error means Weather could
not verify the server. Do not turn off
certificate or server-name checks.
If the clock is right, keep the error
message and consult the Read Me.

## Seed lock: check before recovery

Quit Weather, SeedInstall and all probes.
Check both native OS 9 and Classic.
A lock may belong to a running copy.
Never remove a lock while one is running.

A leftover lock after a confirmed crash
is different from damaged seed data.
Read the lock-only steps in the Read Me.
Do not delete the seed to clear a lock.

## Damaged or uncertain seed state

If a seed record is missing or damaged,
or rotation was interrupted, stop and
follow the seed recovery instructions.
Do not copy an old seed backup back.
Do not reuse another Mac's seed.

Suspected seed exposure needs fresh,
independent secure provisioning. Merely
removing a lock does not repair a seed.
Weather refuses TLS when setup is unsafe.

## Implementation and acceptance notes

- These are proposed page bodies, not a tested final pagination. Keep each native
  page within its existing text area, with an obvious next/close cue. Split long
  sections rather than shrinking text; inspect all pages on Classic/native OS 9.
- Link detailed recovery to the actual bundled Read Me / OS9-SEEDS.md content.
  Keep lock-only removal distinct from invalid records, interrupted rotation and
  possible compromise. No automatic lock removal or weaker TLS is proposed.
- After #184/#185 integrate, add the verified Command-period cancellation and
  system-quit guidance. Until then, do not promise those fixes in a shipped build.
- Preserve Weather / Weather TLS Seed / Weather TLS Lock names. The Unix rename
  in #135 does not change OS 9 files. Native typed Preferences remain #182/v0.2.
- Coordinate final provider wording with #73 and final setup wording with #120.
  No runtime, screenshot, first-run entropy or hardware acceptance is claimed.
