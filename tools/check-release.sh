#!/bin/sh
# Checks release artifacts before they are attached to a draft release. POSIX sh plus hfsutils (hmount/hls).
#   usage: tools/check-release.sh VERSION DIST_TARBALL OS9_DSK [TAG]
# Fails if the tag does not match VERSION, if the tarball is not weather-VERSION.tar.gz, or if the .dsk holds anything
# but the Weather app and documentation (no probes, test apps, seed importer or seed material). Lists the .dsk contents.
version=$1 tarball=$2 dsk=$3 tag=$4
fail=0
bad() { echo "release check: $1"; fail=1; }

[ -n "$version" ] && [ -f "$tarball" ] && [ -f "$dsk" ] || { echo "usage: $0 VERSION TARBALL DSK [TAG]"; exit 2; }
[ -z "$tag" ] || [ "$tag" = "v$version" ] || bad "tag $tag does not match version $version (Makefile VERSION)"
[ "$(basename "$tarball")" = "weather-$version.tar.gz" ] || bad "unexpected tarball name $(basename "$tarball")"
gzip -t "$tarball" || bad "tarball is not valid gzip"
tar tzf "$tarball" | grep -E '(^|/)(weather\.seed|weather-seed\.import)$|\.seed$|\.import$|\.dsk$' && bad "tarball contains seed material or a disk image"

cat "$dsk" > /dev/null || exit 1
hmount "$dsk" > /dev/null || { bad "cannot mount $dsk as HFS (hfsutils)"; exit 1; }
names=$(hls -1 2>/dev/null)
echo "== $dsk contents"; hls -la
humount > /dev/null 2>&1
echo "$names" | grep -qx "Weather" || bad "the .dsk does not contain the Weather app"
for need in LICENSE THIRD_PARTY.md "Read Me"; do echo "$names" | grep -qx "$need" || bad "the .dsk lacks the notice file $need (tools/stage-dsk.sh)"; done
echo "$names" | while IFS= read -r n; do
	case "$n" in
		Weather|"Read Me"*|THIRD_PARTY*|LICENSE*|"Third Party"*) ;;
		*) echo "release check: unexpected file in .dsk: $n"; exit 1 ;;
	esac
done || fail=1
[ "$fail" = 0 ] && echo "release check: ok"
exit $fail
