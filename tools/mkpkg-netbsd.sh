#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Build a NetBSD binary package (belike-VERSION.tgz) for macppc.
# The code is cross-built here; `pkg_create` runs ON maul (it only archives files, so 256 MB is plenty).
# usage: tools/mkpkg-netbsd.sh        result: build/pkg/belike-VERSION.tgz
set -eu
cd "$(dirname "$0")/.."
VERSION=$(sed -n 's/^VERSION *= *//p' Makefile)
PKGNAME=belike-$VERSION
PREFIX=/usr/pkg                         # pkgsrc's prefix on NetBSD
T=netbsd-macppc
XC=${WEATHER_CROSS_PREFIX:-$HOME/scratch/netbsd-xc/tools2/bin/powerpc--netbsd-}
OUT=build/pkg
rm -rf "$OUT"; mkdir -p "$OUT/stage"

make -s TARGET=$T all >/dev/null
make -s TARGET=$T install DESTDIR="$PWD/$OUT/stage" PREFIX=$PREFIX MANDIR=$PREFIX/man >/dev/null
make -s TARGET=$T plist PREFIX=$PREFIX MANDIR=$PREFIX/man > "$OUT/PLIST"

cat > "$OUT/COMMENT" <<'EOC'
Current weather for old Unix machines: command line and CDE-look X11 window
EOC
cat > "$OUT/DESCR" <<'EOC'
belike prints the current conditions for a place; xbelike shows them in a small
CDE-look X11 window with a weather picture, menus, a Location dialog and automatic
refresh every 15 minutes. Data comes from Open-Meteo and MET Norway (CC BY 4.0) over
TLS (built-in BearSSL, no proxy needed). Settings live in ~/.belikerc and are shared
by both programs. Free software under the GNU GPL, version 3 or later.
Source code: https://github.com/jackwillis/weather
EOC

# shared libraries the binaries need (all part of the base system), as pkgsrc's REQUIRES lines
XT=build/$T
req() { # NEEDED names -> absolute paths
	"${XC}readelf" -d "$@" > "$OUT/needed.raw" || return 1
	sed -n 's/.*Shared library: \[\(.*\)\]/\1/p' "$OUT/needed.raw" > "$OUT/needed.names" || return 1
	sort -u "$OUT/needed.names" > "$OUT/needed.sorted" || return 1
	while IFS= read -r lib; do
		case $lib in
		libX11*|libxcb*|libXau*|libXdmcp*) echo "REQUIRES=/usr/X11R7/lib/$lib" ;;
		*) echo "REQUIRES=/usr/lib/$lib" ;;
		esac
	done < "$OUT/needed.sorted"
}
"${XC}gcc" --version > "$OUT/compiler.txt"
CC_VERSION=$(sed -n '1{s/.*) //;s/^/gcc-/;p;}' "$OUT/compiler.txt")
{
	echo "CATEGORIES=misc"
	echo "OPSYS=NetBSD"
	echo "OS_VERSION=11.0"
	echo "MACHINE_ARCH=powerpc"
	echo "PKGTOOLS_VERSION=20091115"
	echo "PKGPATH=misc/belike"
	echo "LICENSE=gnu-gpl-v3"
	echo "BUILD_DATE=$(date -u '+%Y-%m-%d %H:%M:%S +0000')"
	echo "BUILD_HOST=cross-built on Linux x86_64"
	echo "CC_VERSION=$CC_VERSION"
	echo "LOCALBASE=$PREFIX"
	echo "PKGMANDIR=man"
	req "$XT/belike" "$XT/xbelike"
} > "$OUT/BUILD_INFO"
find "$OUT/stage" -type f > "$OUT/stage-files"
sz=0
while IFS= read -r file; do
    bytes=$(wc -c < "$file")
    sz=$((sz + bytes))
done < "$OUT/stage-files"
echo "$sz" > "$OUT/SIZE"

# Check each producer and consumer separately: POSIX sh has no pipefail.
tar -C "$OUT/stage" -cf "$OUT/stage.tar" .
remote=$(maulctl nb 'mktemp -d /tmp/belike-pkg.XXXXXXXX')
case "$remote" in /tmp/belike-pkg.????????) ;; *) echo 'invalid remote package directory' >&2; exit 1;; esac
name=${remote#/tmp/}
case "$name" in *[!a-zA-Z0-9.-]*) echo 'invalid remote directory characters' >&2; exit 1;; esac
maulctl nb "mkdir '$remote/stage'"
maulctl push nb "$OUT/stage.tar" "$name/stage.tar" >/dev/null
maulctl nb "tar -C '$remote/stage' -xf '$remote/stage.tar' && rm -f '$remote/stage.tar'"
for f in PLIST COMMENT DESCR BUILD_INFO SIZE; do maulctl push nb "$OUT/$f" "$name/$f" >/dev/null; done
maulctl nb "cd '$remote' && /usr/sbin/pkg_create -v -B BUILD_INFO -c COMMENT -d DESCR -f PLIST -I $PREFIX -p stage$PREFIX \
    -S SIZE -s SIZE -u root -g wheel $PKGNAME.tgz && ls -l $PKGNAME.tgz"
maulctl nb "cat '$remote/$PKGNAME.tgz'" > "$OUT/$PKGNAME.tgz.part"
tar -tzf "$OUT/$PKGNAME.tgz.part" > "$OUT/package-files"
mv "$OUT/$PKGNAME.tgz.part" "$OUT/$PKGNAME.tgz"
rm -f "$OUT/stage.tar"
maulctl nb "rm -rf '$remote'"
echo "package: $OUT/$PKGNAME.tgz ($(wc -c < "$OUT/$PKGNAME.tgz") bytes)"
