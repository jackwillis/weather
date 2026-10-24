#!/bin/sh
# Build a NetBSD binary package (weather-VERSION.tgz) for macppc.
# The code is cross-built here; `pkg_create` runs ON maul (it only archives files, so 256 MB is plenty).
# usage: tools/mkpkg-netbsd.sh        result: build/pkg/weather-VERSION.tgz
set -eu
cd "$(dirname "$0")/.."
VERSION=$(sed -n 's/^VERSION *= *//p' Makefile)
PKGNAME=weather-$VERSION
PREFIX=/usr/pkg                         # pkgsrc's prefix on NetBSD
T=netbsd-macppc
OUT=build/pkg
rm -rf "$OUT"; mkdir -p "$OUT/stage"

make -s TARGET=$T all >/dev/null
make -s TARGET=$T install DESTDIR="$PWD/$OUT/stage" PREFIX=$PREFIX MANDIR=$PREFIX/man >/dev/null
make -s TARGET=$T plist PREFIX=$PREFIX MANDIR=$PREFIX/man > "$OUT/PLIST"

cat > "$OUT/COMMENT" <<'EOC'
Current weather for old Unix machines: command line and CDE-look X11 window
EOC
cat > "$OUT/DESCR" <<'EOC'
weather prints the current conditions for a place; xweather shows them in a small
CDE-look X11 window with a weather picture, menus, a Location dialog and automatic
refresh every 15 minutes. Data comes from Open-Meteo over TLS (built-in BearSSL,
no proxy needed). Settings live in ~/.weatherrc and are shared by both programs.
EOC

# shared libraries the binaries need (all part of the base system), as pkgsrc's REQUIRES lines
XT=build/$T
req() { # NEEDED names -> absolute paths
	~/scratch/netbsd-xc/tools2/bin/powerpc--netbsd-readelf -d "$@" | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p' | sort -u | while read -r lib; do
		case $lib in
		libX11*|libxcb*|libXau*|libXdmcp*) echo "REQUIRES=/usr/X11R7/lib/$lib" ;;
		*) echo "REQUIRES=/usr/lib/$lib" ;;
		esac
	done
}
{
	echo "CATEGORIES=misc"
	echo "OPSYS=NetBSD"
	echo "OS_VERSION=11.0"
	echo "MACHINE_ARCH=powerpc"
	echo "PKGTOOLS_VERSION=20091115"
	echo "PKGPATH=misc/weather"
	echo "LICENSE=2-clause-bsd"
	echo "BUILD_DATE=$(date -u '+%Y-%m-%d %H:%M:%S +0000')"
	echo "BUILD_HOST=cross-built on Linux x86_64"
	echo "CC_VERSION=$(~/scratch/netbsd-xc/tools2/bin/powerpc--netbsd-gcc --version | head -n 1 | sed 's/.*) //;s/^/gcc-/')"
	echo "LOCALBASE=$PREFIX"
	echo "PKGMANDIR=man"
	req "$XT/weather" "$XT/xweather"
} > "$OUT/BUILD_INFO"
sz=$(find "$OUT/stage" -type f -exec wc -c {} + | awk 'END{print $1}')
echo "$sz" > "$OUT/SIZE"

# ship the staged tree and metadata to maul and package it there
maulctl nb "rm -rf /tmp/wxpkg; mkdir -p /tmp/wxpkg/stage"
tar -C "$OUT/stage" -cf - . | maulctl nb 'tar -C /tmp/wxpkg/stage -xf -'
for f in PLIST COMMENT DESCR BUILD_INFO SIZE; do maulctl push nb "$OUT/$f" "wxpkg/$f" >/dev/null; done
maulctl nb "cd /tmp/wxpkg && /usr/sbin/pkg_create -v -B BUILD_INFO -c COMMENT -d DESCR -f PLIST -I $PREFIX -p stage$PREFIX \
    -S SIZE -s SIZE -u root -g wheel $PKGNAME.tgz && ls -l $PKGNAME.tgz"
maulctl nb "cat /tmp/wxpkg/$PKGNAME.tgz" > "$OUT/$PKGNAME.tgz"
echo "package: $OUT/$PKGNAME.tgz ($(wc -c < "$OUT/$PKGNAME.tgz") bytes)"
