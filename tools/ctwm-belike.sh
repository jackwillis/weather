#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Add xbelike to the ctwm configuration on maul: a "Weather" entry in the root menu and
# XBelike in OccupyAll (shown on every workspace, like XClock). Idempotent, with a backup.
#   sh ctwm-belike.sh          install      (as root: the file is /etc/X11/ctwm/system.ctwmrc)
#   sh ctwm-belike.sh undo     remove exactly what this script added
# env: CTWMRC=path to test on a copy.   Afterwards choose "Restart CTWM" from the root menu.
RC=${CTWMRC:-/etc/X11/ctwm/system.ctwmrc}
set -eu
fail() { echo "FAILED: $*; configuration left unchanged" >&2; exit 1; }
case ${1:-install} in install|undo) action=${1:-install};; *) fail 'use install or undo';; esac
case $RC in /*) ;; *) RC=./$RC;; esac
[ -f "$RC" ] && [ ! -L "$RC" ] || fail "not a regular, non-symlink file: $RC"
links=$(LC_ALL=C ls -ld "$RC") || fail "cannot inspect $RC"
links=$(printf '%s\n' "$links" | awk 'NR == 1 { print $2 }')
[ "$links" = 1 ] || fail 'hard-linked configurations require a manual edit'
stage=
trap '[ -z "$stage" ] || rm -rf "$stage"' 0
trap 'exit 1' 1 2 3 15
prepare() {
    stage=$(mktemp -d "$RC.belike.XXXXXXXX") || fail 'cannot create private sibling directory'
    cp -p "$RC" "$stage/original" || fail 'cannot snapshot configuration'
}
replace() {
    cmp -s "$RC" "$stage/original" || fail 'configuration changed during edit'
    backup=$RC.pre-belike
    if [ -e "$backup" ] || [ -L "$backup" ]; then
        [ -f "$backup" ] && [ ! -L "$backup" ] || fail 'backup path is not a regular non-symlink file'
    else
        cp -p "$stage/original" "$backup" || fail 'cannot create backup'
    fi
    # Preserve file mode/ownership where cp -p supports it, then rename within
    # the same directory/filesystem. A failed write never truncates the original.
    cp -p "$stage/original" "$stage/replacement" || fail 'cannot preserve configuration metadata'
    cat "$stage/text" > "$stage/replacement" || fail 'cannot write replacement'
    mv -f "$stage/replacement" "$RC" || fail 'cannot atomically replace configuration'
}
MARK=maul-belike
prepare
# Refuse incomplete, nested or user-modified owned blocks before deleting any
# range. Similar-looking comments outside these exact markers are unrelated.
awk -v action="$action" '
    /^[[:space:]]*# BEGIN maul-belike[[:space:]]*$/ {
        if (inside) bad = 1
        inside = 1; body = 0; next
    }
    /^[[:space:]]*# END maul-belike[[:space:]]*$/ {
        if (!inside || body != 1) bad = 1
        inside = 0; blocks++; next
    }
    inside {
        if ($0 ~ /^[[:space:]]*" Weather \(xbelike\)"[[:space:]]+!"\/usr\/pkg\/bin\/xbelike &"[[:space:]]*$/) menu++
        else if ($0 ~ /^[[:space:]]*"XBelike"[[:space:]]*$/) occ++
        else bad = 1
        body++
    }
    END {
        if (inside || bad) exit 3
        if (action != "undo" && blocks && (blocks != 2 || menu != 1 || occ != 1)) exit 3
    }
' "$stage/original" || fail 'malformed or modified maul-belike blocks; review manually'
if grep -Eq '^[[:space:]]*# BEGIN maul-belike[[:space:]]*$' "$stage/original"; then
    if [ "$action" = install ]; then echo "already installed in $RC"; exit 0; fi
    sed '/^[[:space:]]*# BEGIN maul-belike[[:space:]]*$/,/^[[:space:]]*# END maul-belike[[:space:]]*$/d' "$stage/original" > "$stage/text" || fail 'cannot remove owned blocks'
    replace
    echo "removed the maul-belike blocks from $RC"
    exit 0
fi
if [ "$action" = undo ]; then echo "nothing to undo in $RC"; exit 0; fi
if ! awk -v mark=$MARK '
	/^menu "NetBSD"/ { sec = "menu" }
	/^OccupyAll/     { sec = "occ" }
	{ print }
	sec == "menu" && !menu_done && /File manager \(xfe\)/ {
		print "    # BEGIN " mark
		print "    \" Weather (xbelike)\"\t\t!\"/usr/pkg/bin/xbelike &\""
		print "    # END " mark
		menu_done = 1
	}
	sec == "occ" && !occ_done && /"XClock"/ {
		print "   # BEGIN " mark
		print "   \"XBelike\""
		print "   # END " mark
		occ_done = 1
		sec = ""
	}
	/^}/ { sec = "" }
	END { if (!menu_done || !occ_done) exit 3 }
' "$stage/original" > "$stage/text"; then fail 'could not find the expected NetBSD menu and OccupyAll anchors'; fi
replace
echo "added Weather to the root menu and XBelike to OccupyAll in $RC (backup: $RC.pre-belike)"
echo 'choose "Restart CTWM" from the root menu to load it'
