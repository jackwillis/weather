#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""Drive only WeatherPrefsUITest's private marker protocol; never answer system dialogs.

Deliver WeatherPrefsUITest first with diagnostics enabled. Assertions run inside
the app against a newly owned test directory; production Preferences and seeds
are not read or modified. Screenshots stay in the requested local directory.
"""
import argparse
import base64
from pathlib import Path
import subprocess
import time

MAULCTL = str(Path.home() / '.local/bin/maulctl')
REMOTE = '/Volumes/MacOS9Lives/weather-dev'


def remote(command):
    return subprocess.run([MAULCTL, 'osx', command], check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          timeout=45).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--capture', nargs='*', type=int, default=list(range(4)))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    remote(f'rm -f {REMOTE}/weather-prefs-ui.ready {REMOTE}/weather-prefs-ui.advance '
           f'{REMOTE}/weather-prefs-ui.log; open {REMOTE}/WeatherPrefsUITest')
    for stage, name in enumerate(('invalid-name', 'save-failure', 'persisted-fields', 'name-only')):
        deadline = time.monotonic() + 180
        reopened = False
        while time.monotonic() < deadline:
            ready = remote(f'cat {REMOTE}/weather-prefs-ui.ready 2>/dev/null || true').strip()
            if ready == str(stage).encode():
                break
            if stage == 0 and not reopened and deadline - time.monotonic() < 110:
                # Classic cold start can drop the first open. Activation only.
                remote(f'open {REMOTE}/WeatherPrefsUITest')
                reopened = True
            time.sleep(1)
        else:
            log = remote(f'cat {REMOTE}/weather-prefs-ui.log 2>/dev/null || true')
            (args.output / 'weather-prefs-ui.log').write_bytes(log)
            raise RuntimeError(f'Preferences stage {stage} did not become ready; see saved log')
        if stage in args.capture:
            shot = remote('/usr/sbin/screencapture -x /tmp/weather-prefs-shot.png; '
                          'openssl base64 < /tmp/weather-prefs-shot.png')
            (args.output / f'{stage:02d}-{name}.png').write_bytes(base64.b64decode(shot))
            print(f'captured {stage}: {name}', flush=True)
        remote(f"printf '{stage}\\n' > {REMOTE}/weather-prefs-ui.advance.tmp; "
               f'mv -f {REMOTE}/weather-prefs-ui.advance.tmp {REMOTE}/weather-prefs-ui.advance')
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        log = remote(f'cat {REMOTE}/weather-prefs-ui.log')
        if b'finished:' in log:
            (args.output / 'weather-prefs-ui.log').write_bytes(log)
            if b'FAIL' in log or b'finished: 0 failures; rc=0' not in log:
                raise RuntimeError('Preferences scenario failed; see saved log')
            print('native Preferences scenario passed', flush=True)
            return
        time.sleep(1)
    raise RuntimeError('Preferences diagnostic did not terminate after Quit')


if __name__ == '__main__':
    main()
