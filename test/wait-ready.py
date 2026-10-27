#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""Wait for a private startup file containing one decimal port/display number."""
import os
import pathlib
import re
import sys
import time

def main():
    path = pathlib.Path(sys.argv[1])
    pid = int(sys.argv[2])
    if pid <= 0:
        raise ValueError('invalid child PID')
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            raise RuntimeError('server exited before becoming ready')
        try:
            data = path.read_text(encoding='ascii')
        except FileNotFoundError:
            data = ''
        if data:
            if not re.fullmatch(r'[0-9]+\n', data) or not 0 <= int(data) <= 65535:
                raise ValueError('invalid startup announcement')
            print(int(data))
            return
        time.sleep(0.02)
    raise RuntimeError('server startup timed out')

if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError) as error:
        print(f'readiness: {error}', file=sys.stderr)
        sys.exit(1)
