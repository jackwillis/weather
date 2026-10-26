#!/bin/sh
# Assemble a deterministic release section on stdout. Never edits source files.
# usage: sh tools/changelog.sh VERSION [fragment-directory]
set -eu
LC_ALL=C
export LC_ALL
die() { echo "changelog: $*" >&2; exit 1; }
[ "$#" -ge 1 ] && [ "$#" -le 2 ] || die 'usage: changelog.sh VERSION [fragment-directory]'
version=$1
case $version in [0-9]*) ;; *) die 'version must start with a digit';; esac
case $version in *[!a-zA-Z0-9.+-]*) die 'invalid version characters';; esac
root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
dir=${2:-$root/changelog.d}
[ -d "$dir" ] || die "fragment directory missing: $dir"
work=$(mktemp -d /tmp/weather-changelog.XXXXXXXX)
trap 'rm -rf "$work"' 0
trap 'exit 1' 1 2 3 15
# Complete and validate the preview before publishing any stdout, so a bad
# later fragment cannot leave a partial release section in a redirected file.
{
    printf '## %s\n\n' "$version"
    for file in "$dir"/*.md; do
        [ -e "$file" ] || [ -L "$file" ] || continue
        name=${file##*/}
        [ "$name" != README.md ] || continue
        [ -f "$file" ] && [ ! -L "$file" ] || die "fragment must be a regular non-symlink file: $file"
        slug=${name%.md}
        case $slug in ''|-*|*-|*[!a-z0-9-]*) die "invalid fragment name: $name";; esac
        awk '
            { sub(/\r$/, "") }
            /^[[:space:]]*$/ { if (started) blanks++; next }
            {
                if (started) for (; blanks > 0; blanks--) print ""
                print
                started = 1
            }
            END { if (!started) exit 3 }
        ' "$file" || die "empty or unreadable fragment: $file"
        printf '\n'
    done
} > "$work/section"
cat "$work/section"
