#!/bin/sh
# Read-only release assembler tests, including native NetBSD sh/awk.
set -eu
TOOL=${1:-$(dirname "$0")/../tools/changelog.sh}
TOOL=$(CDPATH= cd "$(dirname "$TOOL")" && pwd)/$(basename "$TOOL")
work=$(mktemp -d /tmp/weather-changelog-tests.XXXXXXXX)
trap 'rm -rf "$work"' 0
trap 'exit 1' 1 2 3 15
checks=0
pass() { checks=$((checks + 1)); }
reject() {
    if sh "$TOOL" "$@" > "$work/out" 2> "$work/error"; then echo 'FAIL invalid input accepted'; exit 1; fi
    [ ! -s "$work/out" ] || { echo 'FAIL partial output on error'; exit 1; }
    [ -s "$work/error" ] || { echo 'FAIL missing diagnostic'; exit 1; }
    pass
}
mkdir "$work/fragments with spaces"
dir="$work/fragments with spaces"
printf '\n- First.\n\n  Details.\n\n' > "$dir/10-first.md"
printf '\r\n- Second.\r\n\r\n' > "$dir/2-second.md"
printf '# instructions only\n' > "$dir/README.md"
cp "$dir/10-first.md" "$work/original"
printf '## 1.2.3-rc.1\n\n- First.\n\n  Details.\n\n- Second.\n\n' > "$work/expected"
sh "$TOOL" 1.2.3-rc.1 "$dir" > "$work/out"
cmp "$work/out" "$work/expected"; pass
cmp "$dir/10-first.md" "$work/original"; pass
sh "$TOOL" 1.2.3-rc.1 "$dir" > "$work/repeat"
cmp "$work/repeat" "$work/out"; pass
mkdir "$work/empty"
sh "$TOOL" 1.2.3 "$work/empty" > "$work/out"
printf '## 1.2.3\n\n' > "$work/expected-empty"
cmp "$work/out" "$work/expected-empty"; pass
printf '\n \n' > "$dir/zz-empty.md"
reject 1.2.3 "$dir"
rm "$dir/zz-empty.md"
printf '%s\n' '- Invalid name.' > "$dir/bad name.md"
reject 1.2.3 "$dir"
rm "$dir/bad name.md"
ln -s "$dir/10-first.md" "$dir/zz-link.md"
reject 1.2.3 "$dir"
rm "$dir/zz-link.md"
mkdir "$dir/zz-directory.md"
reject 1.2.3 "$dir"
rmdir "$dir/zz-directory.md"
reject 1.2.3 "$work/missing"
reject 'bad version' "$dir"
reject "$(printf '1.2.3\nbad')" "$dir"
reject
reject 1.2.3 "$dir" extra
mkdir -p "$work/repo/tools" "$work/repo/changelog.d" "$work/elsewhere"
cp "$TOOL" "$work/repo/tools/changelog.sh"
printf '%s\n' '- Default directory.' > "$work/repo/changelog.d/default.md"
(cd "$work/elsewhere" && sh "$work/repo/tools/changelog.sh" 2.0.0) > "$work/out"
printf '## 2.0.0\n\n- Default directory.\n\n' > "$work/expected"
cmp "$work/out" "$work/expected"; pass
echo "changelog: $checks checks passed"
