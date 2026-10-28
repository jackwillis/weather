#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""Check the real CLI dump against all bytes in the MET HTTP fixture response."""
import pathlib
import re
import sys

body = (pathlib.Path(__file__).parent / 'fixtures/metno-ok.json').read_bytes()
response = (b'HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n'
            % len(body)) + body
expected = ''.join(chr(c) if 32 <= c < 127 else '.' for c in response)
output = sys.stdin.read()
headers = re.findall(r'^weather: debug: response \((\d+) bytes\):$', output, re.M)
assert headers == [str(len(response))], 'response header must give the complete byte count'
chunks = re.findall(r'^weather: debug: response \[(\d+)\]: (.*)$', output, re.M)
actual = ''
for offset, text in chunks:
    assert int(offset) == len(actual), 'chunks must be contiguous and ordered'
    assert 0 < len(text) <= 240, 'chunk must fit both platform loggers'
    actual += text
assert actual == expected, 'dump must reconstruct the entire sanitized response'
