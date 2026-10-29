## What and why
<!-- One paragraph. Link the issue this closes: "Closes #N". -->

## Checks
- [ ] `make check` and `make check-san` pass (say which were NOT run)
- [ ] New input parsers have mutation coverage in `test/robust.cpp`
- [ ] 32-bit `long` arithmetic is checked before it happens
- [ ] No seeds, imports, credentials or Apple files in the tree
- [ ] A `changelog.d/<pr-or-slug>.md` fragment and relevant docs are updated (or explain why no user-visible note is needed)
<!-- Transition: existing PRs may keep direct CHANGELOG.md edits; no duplicate fragment or new required CI gate. -->

## Verification honesty
<!-- What was run on real hardware, what only in a simulator or by reading, what was not run at all. -->

## Depends on
<!-- Stacked on another PR? Merge order? "None" is fine. -->
