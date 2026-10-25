# RFC: Modern C++ features on Retro68, adopted by evidence

**Status:** Draft research and adoption proposal  
**Date:** 2026-10-25  
**Scope:** Macintosh PowerPC / Retro68 first, with existing NetBSD and Linux builds preserved

## Goal

Write better, safer classic Macintosh software using **appropriate modern C++ language facilities**, without introducing a large C++ runtime or sacrificing binary size, hardware compatibility, build reproducibility, or the current tested behavior of `weather`.

This RFC does **not** propose simply replacing `-std=c++17` with `-std=c++23` in all builds. The project currently fixes C++17 in the top-level Makefile; OS 9 uses Retro68/GCC and CMake. Those are separate toolchains and must be checked independently.

There is no C++24 standard: the relevant published successor to C++20 is C++23 (published in 2024); C++26 is the next revision.

## Research baseline

As of this proposal, Retro68 upstream documents GCC **16.1.0**, newlib **4.6.0**, `libretro`, and distinct classic PowerPC/68k/Carbon targets. GCC supports C++23 mode and substantial C++20/23 language features; this says **nothing by itself** about complete on-target libstdc++, OS services or ABI viability. GCC 16 also changes some C++20+ library ABIs relative to older compilers, so record the toolchain version for tests and releases.

Official references:
- https://github.com/autc04/Retro68
- https://gcc.gnu.org/projects/cxx-status.html
- https://gcc.gnu.org/gcc-16/changes.html
- https://gcc.sourceware.org/onlinedocs/libstdc++/manual/status.html
- https://github.com/autc04/Retro68/issues/281 (existing request for C++20+)
- https://github.com/autc04/Retro68/issues/285, https://github.com/autc04/Retro68/issues/287 (reported locale gaps)
- https://github.com/autc04/Retro68/issues/282 (reported regex gap)

Known `weather` constraints: portable `src/core` and `src/app` use a restricted header whitelist, normally no heap, with `-fno-exceptions -fno-rtti`, and avoid pulling in most libstdc++ on PowerPC. `src/platform/*` owns OS services. `make check-maul` and actual Macintosh execution matter more than successful host compilation.

## Four separate compatibility gates

Test every candidate against:
1. **Parse/compile:** supported by the cross-compiler in a selected language mode.
2. **Link:** builds without unexpected runtime dependencies or undefined references; record exact libraries and symbols.
3. **Execute:** correct operation, destructors, initialization, allocation/error paths and ABI behavior on actual classic PowerPC.
4. **Adopt:** genuinely improves readability/correctness or safety without unacceptable memory, code-size, dependency, test, or portability cost.

Feature-test macros may guide conditional compilation, but **are not evidence of linking or running**.

## Candidate matrix (hypotheses, not verified results)

| Feature / family | Standard | Initial assessment | Required probe |
| --- | --- | --- | --- |
| Concepts and `requires` | C++20 | Attractive, mostly compile-time; useful for buffer and adapter contracts | PowerPC compile plus representative use and size check |
| `consteval` / `if consteval` | C++20 / C++23 | Promising for compile-time parsers, generated tables and invariants | Compile-time assertions and identical runtime behavior |
| `std::span` | C++20 | Potentially useful for bounded buffers; check actual target headers | No-allocation compile, link and run; compare existing `sv` / Buf types |
| `std::source_location` | C++20 | Possible debug/provenance helper; inspect strings and binary growth | Link and size analysis |
| `std::bit_cast`, `std::endian` | C++20 | Valuable for portable bit/byte-order code | 32-bit big-endian hardware checks and no UB |
| Designated initializers | C++20 | Clearer aggregate construction, if useful | Validate no conflict with aggregate/layout assumptions |
| `std::expected` | C++23 | Could express errors but the existing Status/Result design may be smaller | Header + linker + error-path execution; compare ergonomics and size |
| `std::format`, `std::print`, streams, locale, regex | C++20/23 | Defer; runtime and footprint risks | Separate experimental probes only, no initial adoption |
| Filesystem, threads, synchronization | C++17/20+ | OS services and runtime may be missing or mismatched | Avoid assuming support; explicit OS adapter and target tests |
| Modules and coroutines | C++20 | Not a release goal; tooling/runtime and complexity uncertain | Research only if a concrete application need emerges |

No feature above is asserted to work on OS 9 until it passes the matrix. C++20 and C++23 modes should be probed independently; don't assume the presence of a header means its full API is usable.

## Proposed work

### Phase 0: a bounded capability experiment (no product migration)

- Pin and print exact compiler, Retro68 container tag/digest, newlib/libstdc++ version and target triple.
- Add tiny isolated `test/retro68-modern-cpp/` probes for C++17 baseline and candidate C++20/C++23 features. Avoid modifying production build flags at this stage.
- Make both **compile-only** and **linkable executable** tests. Record unresolved symbols, runtime requirements and stripped binary size deltas.
- Execute representative PowerPC programs in Classic-on-Tiger on maul and label it as such; **do not claim native OS 9 verification** without booting native OS 9. Check actual hardware execution of dynamic initialization and destruction, and big-endian arithmetic where applicable.
- Run equivalent probes with host and NetBSD/macppc cross-compiler where supported. Keep the current release baseline unaffected.
- Write a compatibility table of verified/pending/failed/unsupported with command, toolchain and execution environment for each result.

### Phase 1: small useful improvements

Choose at most one or two proven features, perhaps `consteval`, `requires` or `std::span`, if tests demonstrate improved clarity. Make one scoped PR per feature. Preserve current target portability, `-fno-exceptions -fno-rtti`, and size expectations.

A feature requiring C++20 in shared `src/core` would raise the minimum standard for **all** existing targets. That is a separate explicit decision and must not happen as a side effect of a Mac-only experiment. A Mac-specific adapter may use an independently selected language level **only if** its public header/ABI boundary remains compatible with shared consumers.

### Phase 2: upstream contributions when reproducible

For confirmed Retro68-specific breakage, distill a tiny failing test and open an upstream issue or PR rather than inventing a `weather`-only workaround. Check for existing reports first, notably Retro68 #281 (C++20+), #282 (regex), #285/#287 (locale). Distinguish a compiler language bug from libstdc++ link/runtime limitations and newlib/libretro OS gaps.

### Phase 3: reconsider default standard only with a benefit

Revisit raising the project baseline **after** demonstrating a meaningful feature used in production, green host/NetBSD/OS 9 test matrix, stable build images, repeatable source/archive builds and no unacceptable size/memory regression. C++23 is an *option*, not an endpoint or badge.

## Safety and engineering acceptance criteria

- Existing `make check`, sanitizer, 32-bit host CI, NetBSD/macppc hardware checks and OS 9 app build continue to work.
- Probes document **compiler acceptance vs link success vs PowerPC execution** separately.
- No hidden allocations, exceptions, RTTI or newly required hosted OS APIs in portable core.
- No unexamined binary ABI dependencies or unexplained increase in file size / RAM. Record `nm` / `size` (or available equivalents) and link dependencies where practical.
- No `std::filesystem` or `std::thread` assumption for OS 9.
- Treat cross-compiler header availability and compiler-provided feature macros as initial leads, not conformance guarantees.
- Any upstream result should be reproducible without private Apple SDK files, seeds or hardware images.

## Explicit non-goals

- Upgrading or forking GCC simply to change a language-version number.
- Enabling C++23 project-wide before proving value and compatibility.
- Changing BearSSL or TLS protocol implementation.
- Rewriting working error and buffer types solely to adopt standard-library counterparts.
- Building a new exceptions/threads/filesystem compatibility layer for classic Mac OS in this PR.
- Delaying the existing NetBSD/macppc and Mac OS 9 release UX and packaging work.

## Decision requested

Authorize **Phase 0 compatibility research** and publication of a small reproducible feature matrix. Decide on production adoption feature-by-feature, supported by compiler/link/runtime evidence. Keep `weather`'s C++17 baseline unless a separate reviewed change establishes the benefit and fully tested scope.
