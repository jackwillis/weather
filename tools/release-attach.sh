#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Attach the NetBSD package to the DRAFT release the tag workflow created, and add it to SHA512SUMS.
# The release asset is named belike-X.Y.Z-NetBSD-<os version>-macppc.tgz so users see what it is for; pkg_add reads the
# package metadata, not the file name, so installing the renamed file works.
# The package is built on the laptop and maul (`make package-netbsd`; see docs/PROJECT-MANAGEMENT.md); CI cannot build it.
#   usage: tools/release-attach.sh vX.Y.Z [--dry-run]       (needs gh, authenticated; run from the repository root)
# Refuses a published release, a package whose name or contents do not match the tag, and anything but a draft.
tag=$1 dry=$2
case "$tag" in v[0-9]*.[0-9]*.[0-9]*) ;; *) echo "usage: $0 vX.Y.Z [--dry-run]"; exit 2 ;; esac
version=${tag#v}
pkg=build/pkg/belike-$version.tgz
fail() { echo "release-attach: $1"; exit 1; }
run() { if [ "$dry" = "--dry-run" ]; then echo "DRY RUN: $*"; else "$@"; fi; }

[ -f "$pkg" ] || fail "$pkg missing (run: make package-netbsd, with maul booted into NetBSD)"
[ "$(sed -n 's/^VERSION *= *//p' Makefile)" = "$version" ] || fail "Makefile VERSION does not match $tag"
gzip -t "$pkg" || fail "$pkg is not valid gzip"
tar tzf "$pkg" 2>/dev/null | grep -qx '+CONTENTS' || fail "$pkg has no +CONTENTS: not a pkg_create package"
tar tzf "$pkg" 2>/dev/null | grep -q 'share/doc/belike/LICENSE' || fail "$pkg carries no LICENSE"
tar tzf "$pkg" 2>/dev/null | grep -qE '\.(seed|import)$' && fail "$pkg contains seed material"

osver=$(tar xzOf "$pkg" +BUILD_INFO 2>/dev/null | sed -n 's/^OS_VERSION=//p' | head -1)
[ -n "$osver" ] || fail "$pkg has no OS_VERSION in +BUILD_INFO"
asset=belike-$version-NetBSD-$osver-macppc.tgz

state=$(gh release view "$tag" --json isDraft,isPrerelease --jq '"\(.isDraft) \(.isPrerelease)"') || fail "no release for $tag (push the tag first; the workflow creates the draft)"
[ "$state" = "true true" ] || fail "release $tag is not a draft pre-release ($state): refusing to modify it"

work=$(mktemp -d) || exit 1
trap 'rm -rf "$work"' EXIT
gh release download "$tag" --pattern SHA512SUMS --dir "$work" || fail "release has no SHA512SUMS"
cp "$pkg" "$work/$asset"
grep -v "($asset)" "$work/SHA512SUMS" > "$work/new"
printf 'SHA512 (%s) = %s\n' "$asset" "$(sha512sum "$work/$asset" | cut -d' ' -f1)" >> "$work/new"
mv "$work/new" "$work/SHA512SUMS"
cat "$work/SHA512SUMS"
run gh release upload "$tag" "$work/$asset" "$work/SHA512SUMS" --clobber
echo "release-attach: done for $tag (still a draft; Jack publishes)"
