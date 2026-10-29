#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Start xbelike with the X session: adds "xbelike &" to ~/.xinitrc just before the line that execs the window manager.
# Run as yourself (not root):   sh /tmp/xinitrc-belike.sh          install
#                               sh /tmp/xinitrc-belike.sh undo     remove it again
# env: XINITRC=path to test on a copy. Keeps a backup in ~/.xinitrc.pre-belike (made once).
RC=${XINITRC:-$HOME/.xinitrc}
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
LINE='/usr/pkg/bin/xbelike &   # maul-belike'
prepare
if [ "$action" = undo ]; then
    if ! grep -Fxq "$LINE" "$stage/original"; then echo 'nothing to undo'; exit 0; fi
    awk -v line="$LINE" '$0 != line { print }' "$stage/original" > "$stage/text" || fail 'cannot remove owned line'
    replace
    echo "removed xbelike from $RC"
    exit 0
fi
if grep -Fxq "$LINE" "$stage/original"; then echo 'already installed'; exit 0; fi
grep -q '^exec ' "$stage/original" || fail "no exec line in $RC to insert before"
awk -v line="$LINE" '/^exec / && !done { print line; done = 1 } { print }' "$stage/original" > "$stage/text" || fail 'cannot prepare edit'
replace
echo "added xbelike to $RC (backup: $RC.pre-belike); it starts at your next login/startx"
