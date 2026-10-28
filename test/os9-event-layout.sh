#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Inspect the real linked diagnostic: Retro68 can overlap weak mutable storage.
set -eu
nm=${1:?cross nm required}
binary=${2:?linked diagnostic required}
listing=$(mktemp /tmp/weather-event-layout.XXXXXXXX)
trap 'rm -f "$listing"' 0
trap 'exit 1' 1 2 3 15
"$nm" -n -C "$binary" > "$listing"
awk '
    {
        name=$0
        sub(/^[[:xdigit:]]+[[:space:]]+[[:alpha:]][[:space:]]+/, "", name)
        if (name == "wx::SystemEvents::quit_" && $2 == "W") weak=1
        if ($2 !~ /^[bB]$/) next
        if (name == "wx::SystemEvents::quit_") { callback=$1; callbacks++ }
        if (name == "(anonymous namespace)::test_cfg") { config=$1; configs++ }
    }
    END {
        if (callbacks != 1 || configs != 1 || weak || callback == config) {
            print "FAIL OS 9 event storage: callback=" callback ", diagnostic=" config ", weak=" weak
            exit 1
        }
        print "OS 9 event storage: distinct strong callback and diagnostic globals"
    }
' "$listing"
