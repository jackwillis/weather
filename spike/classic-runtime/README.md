# C ABI / CFM feasibility spike

**Research only. No OS 9 shared-library build is claimed.** This isolates the proposed C entry point and client-version semantics *before* tackling Retro68 and Code Fragment Manager loading.

Host smoke test (requires a C99 compiler):

```sh
sh spike/classic-runtime/test-host.sh
```

The script compiles the library and a separate client as **separate translation units**, then checks successful feature negotiation and rejection of an unsupported major API version. This verifies only the C interface shape, **not** binary/PEF dynamic linking.

To prove an actual Mac OS 9 redistributable next:
1. Determine how Retro68's XCOFF linker/MakePEF accepts a shared fragment instead of an application. Inspect PPC export symbols, loader sections, initialization and termination; do not repackage an application PEF and call it a library.
2. Build an actual shared-library PEF with a `cfrg` describing correct PPC fragment name, type and version, then generate or hand-build its import library.
3. Link `client.c` *separately* against the fragment's import stub (not `api.c`). Inspect imports in the client and exports in the library.
4. Install the fragment to the relevant CFM search path on the G4 and run the client under Classic or native OS 9; record Mac OS version and execution path.
5. Repeat with library absent, compatible update, incompatible API version, and other apps open. Prove no fallback statically linked copy masked failures.
6. Capture a reproducible procedure, toolchain digest, artifacts and failure logs. This spike is exploratory: if Retro68 cannot produce the correct fragment, document needed upstream work rather than changing Weather's production build.

Deliberate constraints: C ABI only, no STL or exception boundaries, no pointers with ambiguous ownership, no TLS or real networking, no installer or certificate changes. The `api_size` field anticipates extensibility but **a production ABI still needs target-specific fixed widths, calling-convention validation and stable version rules**.
