# RFC: Open Transport glue provenance, compatibility, and minimum platform support

**Status:** Research and implementation guardrails, 2026-10-25.  
**Scope:** Classic PowerPC networking in Weather / prospective shared runtime (#42). No new supported OS versions are declared by this RFC.

## Short decision

- **Verified target:** the Power Mac G4 running Mac OS 9.2.2 **in Classic under Mac OS X Tiger**, with the production DNS/TCP/TLS path; this is not proof of native-booted OS 9.2.2 or other Mac OS versions.
- **Candidate target:** Mac OS 9.x PowerPC with the required Open Transport libraries, subject to execution tests.
- **Experimental:** Mac OS 8.x PowerPC and selected System 7.x PowerPC installations with compatible OT; do not assert support merely from historical OT availability.
- **Out of scope now:** 68K and older MacTCP-only environments, System 6, and OS X-native networking.
- The current `ot_glue.cc` emulates part of Apple's implicit Open Transport application client setup. It depends on private record implementation details. Before extracting into a redistributable CFM component, evaluate Apple's documented **InContext / OTClassicContext** alternative. Do not make an untested switch in Weather.

## Inventory of the actual code

`src/platform/os9/ot_glue.cc`:
- provides `InitOpenTransport`, `CloseOpenTransport`, `OTOpenEndpoint`, `OTOpenInternetServices`, `OTCloseProvider` and an asynchronous open wrapper;
- maintains a 40-byte client record and passes its address as the extra last argument to selected Open Transport `*Priv` calls;
- checks Gestalt `'otan'`, owns a zero-initialized record and a one-time initialization/at-exit cleanup;
- includes `g_exit_patch_rd = 0xaafe0700`, an observed constant **of unknown meaning**;
- is compiled for PowerPC, where pointers and `UInt32` are 4 bytes. Do not infer that this representation works on 68K, Carbon or other ABIs.

`src/platform/os9/ot_min.h` defines a minimal *independently written* subset of public OT API structures, event/error constants and private-call signatures informed by Apple documentation and tested on hardware.

The code comments specifically acknowledge that portions of the private-record initialization were learned by disassembly of Apple's original static glue. Treat this provenance accurately: it is not sufficient to relabel every field as a documented public API.

## Public references and provenance

- Apple TN1173, *Understanding Open Transport Asset Tracking*: https://leopard-adc.pepas.com/technotes/tn/tn1173.html — explains implicit application client tracking, the `Priv` glue convention and advises **shared libraries to use InContext routines**. It cautions against depending on private client-record structure. Note the historical CarbonLib/OTClassicContext NULL out-parameter caveat.
- Apple sample *OTClassicContext* (2003): https://leopard-adc.pepas.com/samplecode/OTClassicContext/index.html — a published example bridging InContext APIs for InterfaceLib/Classic; lists Mac OS 9, PowerPC and Open Transport 1.1.1+ as runtime requirements. Availability of the sample does not establish that its accompanying source or binaries are freely redistributable.
- Apple Technical Q&A NW64, *Open Transport Versions*: https://leopard-adc.pepas.com/qa/nw/nw64.html — historical OS/OT release combinations.
- Apple Open Transport 1.1.1 technical notes (archived): https://savagetaylor.com/til/TA36111.html — includes System 7.1.x/7.5.x support qualification.
- Retro68: https://github.com/autc04/Retro68 — notes that bundled free Multiversal Interfaces omit OpenTransport and Apple's old Universal Interfaces were distributed under terms that do not permit unqualified redistribution.

The published notes establish **API context**, not proof that our handwritten client record matches all historic OT builds.

## Legal and licensing boundary (not legal advice)

- Interoperability reverse engineering and independently authored compatible interfaces may be defensible under applicable copyright law, but legal risk depends on the source material, governing license, method, copied expression, and distribution context; no blanket clearance is implied.
- Keep Apple's original `OpenTransportAppPPC.o`, `OpenTptInetPPC.o`, SDK headers/binaries and the archived OTClassicContext sample **out of this repository** unless an actual applicable license authorizes the particular distribution.
- Maintain a provenance note distinguishing public API facts, hardware observations, binary-disassembly-derived behavior and original code. Preserve any relevant author/source records locally without committing proprietary disassembly.
- For any broadly distributed standalone library, seek a qualified legal review if uncertain about derivative expression or old SDK license restrictions. Prefer documented APIs where technically feasible.

## Why OS version alone is insufficient

Compatibility is a product of: CPU/ABI (PPC CFM/PEF vs 68K), System version, actual **Open Transport version** and installed component libraries, shared-library symbol availability, Retro68/newlib runtime use, TLS algorithms, accurate UTC clock, required memory and other Toolbox/File Manager/Folder Manager behavior. A published API's age does not validate those other assumptions.

A *candidate* compatibility matrix, not a guarantee:

| OS family | OT historical evidence | Current evidence | Classification |
| --- | --- | --- | --- |
| Mac OS 9.2.2 PPC under Classic | Installed and used | Real G4 DNS/TCP/BearSSL fetch, managed seed, lifecycle probes | **Verified in Classic only** |
| Mac OS 9.2.2 PPC native boot | OT built in | No native-booted test recorded | **Unverified** |
| Mac OS 9.0–9.2.1 PPC | OT available | No execution test | **Candidate** |
| Mac OS 8.0–8.6 PPC | OT shipped with system, version varied | No execution test | **Experimental** |
| System 7.6/7.6.1 PPC | OT 1.1.1 historical release | No execution test | **Experimental** |
| System 7.5.3/7.5.5 PPC | Supported OT 1.1.x configurations | No execution test | **Experimental** |
| System 7.1/7.1.1/7.1.2 PPC | Certain OT 1.1.x deployments supported | No execution test, other minimum-OS issues | **Research only** |
| Other System 7 / 68K / System 6 | Unsupported or different stacks | None | **Outside active scope** |

The fact that OT 1.1.x was offered for some 7.x configurations does not establish that **Weather's binary** will launch there. Conversely, a feature not provided by early OT may require a minimum version higher than the system's nominal version.

## Documented-context implementation investigation (high priority)

1. Read TN1173 and OTClassicContext source **for evaluation only**, track license and version requirements; identify the exact `InContext` entry points and symbol names exposed by available Retro68 import libraries.
2. Prototype application client initialization and endpoint, service-provider, async-open and close lifecycles using an explicit context. Do **not** mix the implicit client record with explicit contexts on one provider without proof.
3. Confirm context lifetime/ownership with repeated initialization, cancellation while DNS is pending, provider shutdown, reinitialization, late callbacks and library unload. For future CFM hosting, applications and shared fragments must not assume the same global client.
4. Test against the current implementation using the same hardware fixtures, certificate-verified TLS, managed seed, and event-pumping tests. Check every error/timeout path.
5. If the documented path requires unavailable old APIs or new import stubs, record blocker and retain the existing working glue with explicit supported-version restrictions. Do **not** claim that changing to InContext is automatically safer without testing.

## Technical hardening and probes

- Assert the target-specific client-record size and offsets at build time (`sizeof`, `offsetof`) **only for the proven PPC 32-bit ABI**. Layout checks prove internal assumptions, **not** validity of private fields on different OS versions.
- Record the unexplained `0xaafe0700` initialization constant and investigate it before any portable/shared-library extraction; never claim its purpose without evidence.
- Collect runtime metadata via supported Gestalt/OT version facilities, where verified. A startup diagnostics report should print System version, OT version, CPU/ABI, Classic-vs-native when reliably determinable, memory and tested capability flags. Avoid guessing from System version alone.
- Use feature detection or explicit safe-version gates for symbols whose availability varies. Don't assume checking `'otan'` guarantees every `*Priv` call is present. Missing dynamically imported functions may prevent application launch **before runtime checks**; investigate weak/dynamic linkage as needed.
- Introduce compile-only and host tests for declaration sizes, callback conventions, ownership and deadline behavior, with hardware/emulation tests for each new claimed version. No untested import-signature changes.
- Keep OS 9 platform compatibility checks separate from certificate-store work (#40) and CFM library proof (#42).

## Suggested acceptance sequence

**P0 — documentation and guardrails (this PR):** publish provenance, version matrix, original-code scope and explicit confidence levels; add PPC-only compile-time layout assertions.

**P1 — documented API spike:** separate experimental Retro68 target, no Weather behavior change. Demonstrate `OTClassicContext`/InContext viability and its actual minimum runtime.

**P2 — platform probe:** small standalone PowerPC diagnostic app reporting System/OT versions and exercising DNS/TCP without TLS secrets. Test first on current G4; subsequently emulated 9.0, 8.6, 8.1, 7.6.1 if legal install media and reliable emulators are available. Results must state actual OS build, OT version, hardware/emulator and test coverage.

**P3 — rollout decision:** select supported runtime ranges from empirical evidence. Keep existing glue on versions where it is the only tested method; consider an InContext migration only after regression and lifecycle coverage. Update release metadata, installer prerequisites, help/errors and documentation.

**P4 — shared runtime:** only after CFM and context ownership are separately proven, publish the versioned developer API and redistributable packages. No premature promise of universal classic Mac OS support.

## Ship policy

For Weather 0.1, claim **Mac OS 9.2.2 PowerPC, verified under Classic on the G4**, not “System 7–9 compatible.” Run native OS 9.2.2 and other 9.x environments before broadening. Unsupported platforms may work; unverified is not the same as broken.

## Compiler verification, 2026-10-26

The full PowerPC OS9 cross-build passes with these layout assertions. `make check-ot-layout` also compiles the actual glue, then requires deliberately changed record size and flag offset to fail their expected assertions. It uses private copies of our source and no copied SDK files. The ineffective ordinary function-address null test is removed; this does not add weak imports or detect a missing CFM import before program load. Evidence and runtime limits: `docs/verification/ot-layout-2026-10-26.txt`.
