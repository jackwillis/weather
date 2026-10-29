#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Copy a built Mac OS 9 disk image and add the notices a release must carry: LICENSE, THIRD_PARTY.md and a Read Me.
# Needs hfsutils (hmount/hcopy/humount). The input image is not modified.
#   usage: tools/stage-dsk.sh IN.dsk OUT.dsk VERSION      (run from the repository root)
in=$1 out=$2 version=$3
[ -f "$in" ] && [ -n "$out" ] && [ -n "$version" ] || { echo "usage: $0 IN.dsk OUT.dsk VERSION"; exit 2; }
for f in LICENSE THIRD_PARTY.md; do [ -f "$f" ] || { echo "stage-dsk: run from the repository root ($f missing)"; exit 1; }; done
readme=$(mktemp) || exit 1
trap 'rm -f "$readme"; humount >/dev/null 2>&1' EXIT
cat > "$readme" <<EOF
Weather $version for Mac OS 9

A small weather window. Free software: GNU General Public License, version 3 or
(at your option) any later version. There is NO WARRANTY. See LICENSE.

Source code (exactly this version):
  https://github.com/jackwillis/weather/releases/tag/v$version
  (the belike-$version.tar.gz attached to that release)

Credits and third-party notices: THIRD_PARTY.md. Weather data comes from
Open-Meteo.com and MET Norway (CC BY 4.0).

Using TLS on Mac OS 9 needs a seed provisioned from another computer first.
No seed is included here. See docs/OS9-SEEDS.md in the source.

If Weather says the TLS seed is locked, missing or damaged:

  Weather TLS Lock left over. Quit Weather, SeedInstall and any probe, in
  native OS 9 and in Classic. If nothing is running and only the file
  "Weather TLS Lock" is left (after a crash), delete just that file from the
  Preferences folder. Your seed is intact. Never delete a lock that a
  running copy holds, and never delete the seed to clear a lock.

  Seed missing or damaged, or "Weather TLS Next" present. Weather refuses
  TLS rather than guess. Quit everything that uses the seed, remove only the
  three Weather TLS files (Seed, Next, Lock) from the Preferences folder, and
  install a newly generated seed from another computer (docs/OS9-SEEDS.md).
  Do not copy back an old seed or use another Mac's seed.

  Seed possibly exposed (copied, backed up, shared, or the disk cloned).
  Removing a lock does not help. Replace the seed with a fresh one made on
  another computer.

Never turn off certificate or server-name checking to get around an error.

Copyright (c) 2026 Jack Willis.
EOF
cp "$in" "$out" || exit 1
hmount "$out" >/dev/null || { echo "stage-dsk: cannot mount $out"; exit 1; }
hcopy -t LICENSE ":LICENSE" && hcopy -t THIRD_PARTY.md ":THIRD_PARTY.md" && hcopy -t "$readme" ":Read Me" || { echo "stage-dsk: copy failed"; exit 1; }
for f in LICENSE "Read Me" THIRD_PARTY.md; do hattrib -t TEXT -c ttxt ":$f"; done   # open in SimpleText, not as an unknown UNIX file
hls -la
