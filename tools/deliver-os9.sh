#!/bin/sh
# Run in the FOREGROUND on Mac OS X 10.4. Copies a Retro68 disk image's APPL
# with its resource fork. deliver.done indicates success, not merely termination.
# usage: sh deliver-os9.sh /tmp/App.dsk AppName
# Defaults: /Volumes/MacOS9Lives/weather-dev and /tmp/deliver.{log,done}.
# WEATHER_DELIVER_DEST / WEATHER_DELIVER_STATE override them for isolated tests.
set -eu
dest=${WEATHER_DELIVER_DEST:-/Volumes/MacOS9Lives/weather-dev}
state=${WEATHER_DELIVER_STATE:-/tmp}
log=$state/deliver.log
done_file=$state/deliver.done
rm -f "$done_file"
dsk=${1:?disk image required}
app=${2:?application name required}
case $app in .|..|*/*) echo 'application name must be a single file name' >&2; exit 1;; esac
[ -d "$dest" ] || { echo "destination directory missing: $dest" >&2; exit 1; }
work=$(mktemp -d "$state/weather-deliver.XXXXXXXX")
stage=
dev=
cleanup() {
    status=$?
    if [ -n "$dev" ]; then hdiutil detach "$dev" >> "$log" 2>&1 || :; fi
    [ -z "$stage" ] || rm -rf "$stage"
    rm -rf "$work"
    trap - 0
    exit "$status"
}
trap cleanup 0
trap 'exit 1' 1 2 3 15
read_device() { awk '$1 ~ /^\/dev\/disk[0-9]+(s[0-9]+)?$/ { print $1; exit }' "$work/attach"; }
{
    if hdiutil attach "$dsk" -imagekey diskimage-class=CRawDiskImage -nomount > "$work/attach"; then
        dev=$(read_device)
    else
        dev=$(read_device)
        echo 'disk image attachment failed'
        exit 1
    fi
    [ -n "$dev" ] || { echo 'no attached disk device reported'; exit 1; }
    echo "device=$dev"
    diskutil mount "$dev"
    diskutil info "$dev" > "$work/info"
    vol=$(sed -n 's/^ *Mount Point: *//p' "$work/info")
    case $vol in /*) ;; *) echo 'mounted volume path missing'; exit 1;; esac
    echo "volume=$vol"
    [ -f "$vol/$app" ] || { echo "source $vol/$app missing; destination left untouched"; exit 1; }
    [ ! -d "$dest/$app" ] || { echo 'destination application is a directory'; exit 1; }
    stage=$(mktemp -d "$dest/.weather-deliver.XXXXXXXX")
    ditto -rsrc "$vol/$app" "$stage/$app"
    hdiutil detach "$dev"
    dev=
    mv -f "$stage/$app" "$dest/$app"
    ls -l "$dest/$app"
    : > "$work/done"
    mv -f "$work/done" "$done_file"
} > "$log" 2>&1
