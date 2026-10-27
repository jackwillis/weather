#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Compile two C translation units and test a provisional ABI without touching Weather builds.
set -eu
CC=${CC:-cc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
"$CC" -std=c99 -Wall -Wextra -Werror -c spike/classic-runtime/api.c -o "$tmp/api.o"
"$CC" -std=c99 -Wall -Wextra -Werror -c spike/classic-runtime/client.c -o "$tmp/client.o"
"$CC" "$tmp/client.o" "$tmp/api.o" -o "$tmp/client"
"$tmp/client"
printf '%s\n' 'C ABI host smoke test: PASS (does not validate CFM loading)'
