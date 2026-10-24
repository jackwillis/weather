# Vendored third-party code

## bearssl/ (MIT, see bearssl/LICENSE.txt and ../THIRD_PARTY.md)
Pinned copy of https://www.bearssl.org/git/BearSSL at commit
`7bea48e5e850ab4cafbe68d3765cdaba13a86d6f` (2026-04-06, "Fixed bug in handling incoming...").
Exported with `git archive`; the test suite, samples, Doxygen config and the prebuilt `T0Comp.exe`
(only needed to regenerate T0 output, which is already committed in `src/`) were removed.
Local modifications: none. To update: export a newer commit the same way, record the hash here,
run `make check check-maul`.
