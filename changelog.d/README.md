# Changelog fragments

For a new change, add a short Markdown entry in its own file, such as
`changelog.d/87.md` or `changelog.d/location-search.md`. Use lowercase ASCII
letters, digits and hyphens; start and end the name with a letter or digit.
Prefer a brief bullet explaining the visible change and why it matters.
Include the issue or PR number when useful. This README is ignored by the tool.

Preview a release section from the current fragments:

```sh
sh tools/changelog.sh 0.1.0 > /tmp/weather-release-section.md
```

The script uses POSIX sh/awk and writes only stdout. It reads files in lexical
filename order under the C locale (so `10.md` comes before `2.md`), strips leading
and trailing blank lines, normalizes CRLF and preserves internal Markdown.
Empty or malformed fragments fail with a diagnostic and no section on stdout.
With no fragments, it prints only the version heading. A second argument selects
a different fragment directory for previews or tests.

At release time, review the generated section, combine it with the existing
version's legacy entries in `CHANGELOG.md`, and remove consumed fragments in the
same release commit. Existing changelog text is never parsed, regenerated or
deleted by this tool. Do not replace the whole changelog with the preview.

During the transition, older open PRs may keep their direct `CHANGELOG.md` edits.
No fragment check blocks them, and they do not need duplicate fragments. New PRs
should use separate fragment files. Releases still require the owner's approval.
