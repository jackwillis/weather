# OS 9 entropy measurement plan (#120 / #121)

Status (2026-10-28): initial Classic and native OS 9 captures are available, and
the separate NIST assessment tools have passed their reference self-tests. No
validated entropy estimate, source credit or collection threshold exists. Production
still requires the managed provisioned seed; this work does not weaken or bypass
that requirement. It never opens seed, Preferences or user-content files.

## What the probe measures

EntropyProbe is a separate target under WEATHER_BUILD_DIAGNOSTICS. It samples
back-to-back Microseconds call latency, WaitNextEvent latency, Delay(1) latency,
reads of its own newly created 512-byte zero-filled file, input-event kind/time
and mouse position. It never records key values, event-message payloads or file
contents. The disk test is a repeated warm-cache read, not a claim of physical
disk noise. Microseconds values are raw measurements, not seed bytes.

The first 30 active seconds are labelled idle; the next 60 are labelled
interaction. Run a fully idle session as a negative control too. Sampling pauses
when the app is backgrounded or its window is not frontmost. Its fixed 8192-row
buffer is saved only after capture, so CSV writes are outside the measurement
loop. A 90-second run is exploratory and far smaller than a complete source
assessment dataset. There is no entropy progress meter, seed conditioning or
network initialization in this executable.

Output is an exclusively created Entropy-<process>-<ticks>.csv in the launch
folder. Existing paths cause refusal. A uniquely owned EP-input-... file is
created for the disk measurement and deleted afterward; no unrelated path is
opened. Cancel/early close saves an incomplete capture, which the analyzer
refuses. A crash can leave the owned scratch/output files for manual inspection.

CSV version 1 has a fixed header, twelve bounded integer fields and a checked
completion/count footer. Fields are index, phase, raw ticks, raw microseconds,
call/wait/delay/disk latency in microseconds, event kind/time and local mouse x/y.
UINT64_MAX marks an invalid latency. The format excludes character codes.
Neither raw CSV nor analysis output belongs in the repository by default.

## Owner hardware runbook

Build on Linux, never on maul. Enable diagnostics explicitly in the Retro68
configuration (make os9 by itself retains the existing CMake option):

```sh
mkdir -p build/os9
podman run --rm -v "$PWD":/src:Z -w /src/build/os9 ghcr.io/autc04/retro68 sh -c \
 'cmake /src/os9 -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake -DWEATHER_BUILD_DIAGNOSTICS=ON && make EntropyProbe'
```

With the owner logged into Tiger and Classic available:

```sh
maulctl push osx build/os9/EntropyProbe.dsk EntropyProbe.dsk
maulctl push osx tools/deliver-os9.sh deliver-os9.sh
maulctl osx 'sh /tmp/deliver-os9.sh /tmp/EntropyProbe.dsk EntropyProbe'
maulctl osx 'open /Volumes/MacOS9Lives/weather-dev/EntropyProbe'
```

Run the delivery in the foreground. The owner watches the probe window and does
one of these labelled trials; never interact with system dialogs through an
agent. Do not type private text; arbitrary nonsecret keys suffice.

1. Idle: no mouse or keyboard activity for the entire run.
2. Manual: first phase idle, then varying mouse movement and key timing only in
   the probe window. No keystroke content is logged.
3. Scripted control: use an owner-approved deterministic event feeder, documenting
   the tool and cadence. No event-injection driver is included in this PR.

Repeat separate launches. Record machine/model, OS version, boot/launch number,
foreground/background interruptions and other workload outside the raw CSV.
Do not pool Classic and native OS 9 data. After Classic trials, the owner boots
native OS 9 and opens the same app manually, repeats the controls, then boots
Tiger to retrieve files. Reboot/privileged steps remain the owner's.

List only the probe's output files:

```sh
maulctl osx 'ls /Volumes/MacOS9Lives/weather-dev/Entropy-*.csv'
```

Retrieve the chosen actual filename with maulctl osx plus openssl base64 (Tiger
has no base64 command), decode on Linux, and preserve the original capture. Run:

```sh
python3 tools/analyze-os9-entropy.py /tmp/ACTUAL-CAPTURE.csv --environment classic --run idle
```

Use native-os9 and the actual run label for other trials. Keep captures outside
the checkout. There is no automatic upload, seed installation or seed deletion.

## Analysis and decision gate

The supplied Python tool reports distribution concentration, identical runs,
zero frequency, lag-one autocorrelation, raw event/mouse timing gaps and observed
rates separately by phase and environment. Its empirical marginal bit statistic
is merely -log2(observed most-common fraction): deterministic counters and
alternation can score highly. It is not a conservative entropy bound. All output
explicitly gives zero credited bits and labels itself exploratory only. Invalid
latencies and clock regression are reported; incomplete/malformed/oversized
captures are refused. Host tests cover constant and predictable inputs and
mutate every character of a fixture capture.

A later assessment needs a declared source/noise model, reproducible fixed
quantization, sufficient unconditioned samples, non-IID estimators and restart
checks. Do not select whichever low bits happen to look best after seeing data,
add correlated sources as independent entropy, or treat passing statistical tests
as proof of unpredictability. Preserve trial boundaries, including idle/scripted
controls, and state observer effects and differences between environments.

The [NIST SP 800-90B assessment implementation](https://github.com/usnistgov/SP800-90B_EntropyAssessment)
provides non-IID and restart tools; its README was checked 2026-10-27. Its NIST
notice permits use/distribution with the notice intact and acknowledgement.
It is not vendored or linked here. The 2026-10-28 separate Linux build used
revision `87c104d0ed4cbc96103e7b8b38d6f2c7e0a6b289`, retained the full upstream
notice, and passed all 11 reference comparisons (maximum delta about 1.01e-13).
This verifies the assessment executable, not the Weather noise sources. Follow [SP 800-90B](https://csrc.nist.gov/pubs/sp/800/90/b/final)
for dataset and restart requirements; this diagnostic is not a certification or
a replacement for that assessment.

| Environment | Captures | Credited bits/sample | Threshold |
|---|---|---|---|
| Classic on Tiger | 4 exploratory captures; 904–3093 rows each | 0 credited; no bound established | not selected |
| Native OS 9 | 3 exploratory captures; 2695–3117 rows each | 0 credited; no bound established | not selected |

See [the dated readiness record](verification/entropy-assessment-readiness-2026-10-28.md)
for capture provenance, controls, self-test reproduction and the remaining data
requirements. Short captures have not been presented as NIST source estimates.

Only reviewed native measurements can inform #120's source/credit/threshold
choice. Classic results cannot substitute for native OS 9. Other G3/G4 models,
cold boots, long-term drift and adversarial input remain separate limits.

## Event-timing pilot (#193; owner decision 2026-10-28)

The v0.1 collector (#120) is option B of the #121 discussion: credit only discrete
human input at a conservative per-event rate, aim for 128 credited bits, and mix every
other observation at zero credit. The EntropyProbe above measures timers in a polling
loop and stores event time only in ticks (1/60 s); it cannot say how much the interval
between key presses and clicks carries. A separate pilot measures that. Nothing below
is implemented yet and no credit exists.

### Capture design (CSV version 2, event-driven, diagnostics only)
- One row per input event, not per loop: index, phase, event kind (key-down, key-up
  where available, auto-repeat, mouse-down, mouse-up), `EventRecord.when` in ticks,
  `Microseconds()` taken immediately after `WaitNextEvent` returns, microseconds since
  the previous event, and mouse position for mouse events only. Auto-repeat is recorded
  but never credited. Key identities, characters and modifier combinations are not
  recorded.
- A tight `WaitNextEvent(..., sleep=0)` loop with no `Delay` and no disk I/O inside it.
  The CSV is written only after the capture. A once-a-second heartbeat row exposes
  gaps, backgrounding and stalls.
- Key-up events are probably masked out by the system unless `SetEventMask(everyEvent)`
  is called. This is unverified; confirm on hardware before relying on key-up timing.
- Mouse movement is summarised (move events per second, gesture boundaries at button
  changes and pauses over 250 ms) so credit per raw move event can be compared with
  credit per gesture.
- Ends at a fixed event count or time limit with a checked completion footer. Native
  OS 9 and Classic are separate records. No seed, Preferences, network or user-content
  access.

### Symbols and statistics, fixed before any new capture is scored
The symbol is the interval between successive credited events, reduced by each of
three fixed rules, all reported and none chosen after seeing results:
1. floor(interval in microseconds / 1000), clipped to 0..1023
2. floor(interval in microseconds / 100), clipped to 0..4095
3. interval in microseconds modulo 256

Event classes are reported separately (key-down, mouse-down, distinct gesture) and
fields are never summed as independent sources. Statistics: most-common-value upper
bound, lag-1 and lag-2 correlation, run lengths, interval distribution. The NIST
SP 800-90B estimators built under #186 are a sanity check only; a few thousand
intervals are far below a validation dataset and no bound is claimed.

Controls: idle (expect almost no events), a metronome tap (a person tapping a steady
beat, to show periodic input is detected and earns little or nothing), and a scripted
deterministic stream if the owner approves a feeder.

### Output of the pilot
A per-event credit well below the most conservative estimate (at least 4x lower, to be
reviewed), the number of events needed for 128 bits, health-test thresholds (minimum
interval spread, maximum repeat share, minimum distinct gestures), the expected dialog
duration, and a written list of what is not established (single machine, few sessions,
no cold-boot restart statistics, correlated mouse movement).

### Sessions
At least three per environment (native OS 9 and Classic), 3 to 4 minutes each, on
different boots or days, typing nonsense and clicking, plus one idle and one metronome
control. If possible one session by a second person, since one person's style would
overfit the credit. Record model, OS version, launch number, interruptions and workload
beside the CSV; raw captures stay outside the repository.

## First-run design boundary (#120)

After #121, a portable collector would separate source health/credit policy from
a standard cryptographic conditioner and the existing managed-seed store. The
window would collect only before the first fetch, outside OT callbacks and nested
network pumps; Cancel or insufficient credit would leave TLS unavailable. No
collector or conditioner is implemented in this PR and no threshold is guessed.

The installation design must keep exclusive ownership, checked creation and
volume flushes, reject weak seed records, and expose collected/provisioned
provenance. A separate versioned provenance record and its failure/rollback
semantics need review before implementation. Provisioned seeds retain precedence;
no silent combination/replacement or live-lock removal is allowed. Cloning,
rollback, compromise recovery and physical power-loss durability stay explicit
limits. A human-approved source model, conditioner vectors, health tests,
mutation tests and unchanged seed-store fault tests precede production wiring.
