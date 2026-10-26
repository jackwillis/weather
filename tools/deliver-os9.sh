#!/bin/sh
# Runs ON the Mac (OS X 10.4): copy an app out of a Retro68 .dsk image into MacOS9Lives:weather-dev with its resource fork.
# usage: sh deliver-os9.sh /tmp/App.dsk AppName      (writes /tmp/deliver.done when finished, log in /tmp/deliver.log)
# Weather is the native window; WeatherConsole is the console diagnostic.
# WeatherUITest is a separate optional self-running test app. Quit an old copy
# normally before replacing it; force-quitting can leave a managed TLS seed lock.
# Notes: plain `hdiutil attach` hung/failed here ("Device not configured") while a stale hdiutil was running; attaching with
# -nomount and mounting through diskutil is the reliable path. Run it in the FOREGROUND over ssh: under nohup/& the attach fails. A stuck `hdiutil attach` needs kill -9.
dsk=$1 app=$2 dest=/Volumes/MacOS9Lives/weather-dev
rm -f /tmp/deliver.done
{
  dev=$(hdiutil attach "$dsk" -imagekey diskimage-class=CRawDiskImage -nomount | awk '{print $1; exit}')
  echo "device=$dev"
  diskutil mount "$dev"
  vol=$(diskutil info "$dev" | sed -n 's/^ *Mount Point: *//p')
  echo "volume=$vol"
  if [ -f "$vol/$app" ]; then ditto -rsrc "$vol/$app" "$dest/$app"; else echo "source $vol/$app missing; destination left untouched"; fi
  hdiutil detach "$dev"
  ls -l "$dest"
} > /tmp/deliver.log 2>&1
touch /tmp/deliver.done
