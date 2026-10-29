#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Portable smoke checks for the standalone editors, on private copies only.
# Also runs with NetBSD's native sh/awk: pass the directory holding both scripts.
set -eu
TOOLS=${1:-$(dirname "$0")/../tools}
work=$(mktemp -d /tmp/belike-session.XXXXXXXX)
trap 'rm -rf "$work"' 0
trap 'exit 1' 1 2 3 15
cat > "$work/ctwm" <<'EOF'
menu "NetBSD"
{
    "File manager (xfe)" !"xfe &"
}
OccupyAll
{
    "XClock"
}
# keep the tail
EOF
cp "$work/ctwm" "$work/original"
exec 3< "$work/ctwm"
CTWMRC="$work/ctwm" sh "$TOOLS/ctwm-belike.sh"
cat <&3 > "$work/old-fd"
exec 3<&-
cmp "$work/old-fd" "$work/original"
cmp "$work/ctwm.pre-belike" "$work/original"
cp "$work/ctwm" "$work/installed"
CTWMRC="$work/ctwm" sh "$TOOLS/ctwm-belike.sh"
cmp "$work/ctwm" "$work/installed"
CTWMRC="$work/ctwm" sh "$TOOLS/ctwm-belike.sh" undo
cmp "$work/ctwm" "$work/original"
echo 'ok ctwm atomic install/idempotence/undo'
printf '# BEGIN maul-belike\n"XBelike"\n# keep remainder\n' >> "$work/ctwm"
cp "$work/ctwm" "$work/malformed"
if CTWMRC="$work/ctwm" sh "$TOOLS/ctwm-belike.sh" undo; then echo 'FAIL malformed block accepted'; exit 1; fi
cmp "$work/ctwm" "$work/malformed"
echo 'ok malformed ctwm block refused unchanged'
printf '# maul-belike notes\necho maul-belike\nexec ctwm\n' > "$work/xinit"
cp "$work/xinit" "$work/xinit-original"
XINITRC="$work/xinit" sh "$TOOLS/xinitrc-belike.sh"
XINITRC="$work/xinit" sh "$TOOLS/xinitrc-belike.sh" undo
cmp "$work/xinit" "$work/xinit-original"
echo 'ok xinit install/undo preserves unrelated marker text'
mkdir "$work/bin"
printf '#!/bin/sh\nexit 29\n' > "$work/bin/mv"
chmod 700 "$work/bin/mv"
cp "$work/original" "$work/ctwm"
if CTWMRC="$work/ctwm" PATH="$work/bin:$PATH" sh "$TOOLS/ctwm-belike.sh"; then echo 'FAIL failed rename accepted'; exit 1; fi
cmp "$work/ctwm" "$work/original"
for temporary in "$work/ctwm.belike."*; do
    [ ! -e "$temporary" ] || { echo 'FAIL temporary edit leaked'; exit 1; }
done
echo 'ok failed rename leaves original intact and cleans private temp'
echo 'session edits: 4 smoke checks passed'
