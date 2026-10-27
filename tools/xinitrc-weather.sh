#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Start xweather with the X session: adds "xweather &" to ~/.xinitrc just before the line that execs the window manager.
# Run as yourself (not root):   sh /tmp/xinitrc-weather.sh          install
#                               sh /tmp/xinitrc-weather.sh undo     remove it again
# env: XINITRC=path to test on a copy. Keeps a backup in ~/.xinitrc.pre-weather (made once).
RC=${XINITRC:-$HOME/.xinitrc}
LINE='/usr/pkg/bin/xweather &   # maul-weather'
[ -f "$RC" ] || { echo "no such file: $RC"; exit 1; }
if [ "$1" = undo ]; then
	grep -q 'maul-weather' "$RC" || { echo "nothing to undo"; exit 0; }
	grep -v 'maul-weather' "$RC" > "$RC.new" && cat "$RC.new" > "$RC" && rm -f "$RC.new"
	echo "removed xweather from $RC"; exit 0
fi
grep -q 'maul-weather' "$RC" && { echo "already installed"; exit 0; }
grep -q '^exec ' "$RC" || { echo "FAILED: no 'exec ...' line in $RC to insert before; nothing changed"; exit 1; }
awk -v line="$LINE" '/^exec / && !done { print line; done = 1 } { print }' "$RC" > "$RC.new" || { rm -f "$RC.new"; echo FAILED; exit 1; }
[ -f "$RC.pre-weather" ] || cp -p "$RC" "$RC.pre-weather"
cat "$RC.new" > "$RC" && rm -f "$RC.new"
echo "added xweather to $RC (backup: $RC.pre-weather); it starts at your next login/startx"
