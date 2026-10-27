#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""Startup/isolation regression: no production network, ports or credentials."""
import os
import pathlib
import shlex
import tempfile
import select
import socket
import subprocess
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent

class FixtureStartup(unittest.TestCase):
    def test_two_servers_announce_distinct_live_ports(self):
        servers = []
        try:
            ports = []
            for mode in ('500', '404'):
                proc = subprocess.Popen([sys.executable, str(HERE / 'fixture_server.py'), mode, '0'],
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                servers.append(proc)
                self.assertTrue(select.select([proc.stdout], [], [], 2)[0], 'fixture did not announce its bound port')
                line = proc.stdout.readline().strip()
                self.assertTrue(line.isascii() and line.isdigit(), repr(line))
                ports.append(int(line))
            self.assertNotEqual(ports[0], ports[1])
            for port, status in zip(ports, (b'500', b'404')):
                with socket.create_connection(('127.0.0.1', port), timeout=2) as conn:
                    conn.sendall(b'GET / HTTP/1.0\r\n\r\n')
                    self.assertIn(status, conn.recv(1024).split(b'\r\n')[0])
        finally:
            for proc in servers:
                proc.terminate()
                try:
                    proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
                proc.stdout.close()
                proc.stderr.close()

    def test_readiness_refuses_malformed_and_stale_announcements(self):
        with tempfile.TemporaryDirectory() as tmp:
            ready = pathlib.Path(tmp) / 'ready'
            ready.write_text('not-a-port\n')
            command = [sys.executable, str(HERE / 'wait-ready.py'), str(ready), str(os.getpid())]
            result = subprocess.run(command, capture_output=True, text=True, timeout=2)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('invalid startup announcement', result.stderr)
            dead = subprocess.Popen([sys.executable, '-c', 'pass'])
            dead.wait()
            ready.write_text('12345\n')
            command[-1] = str(dead.pid)
            result = subprocess.run(command, capture_output=True, text=True, timeout=2)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('exited before becoming ready', result.stderr)

    def test_feature_suite_fails_closed_when_fixture_startup_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            wrapper = root / 'python3'
            wrapper.write_text('#!/bin/sh\ncase "$1" in */fixture_server.py) echo forced-fixture-failure >&2; exit 1;; esac\nexec ' + shlex.quote(sys.executable) + ' "$@"\n')
            wrapper.chmod(0o755)
            binary = root / 'weather'
            binary.write_text('#!/bin/sh\necho WEATHER-MUST-NOT-RUN\n')
            binary.chmod(0o755)
            env = dict(os.environ, PATH=str(root) + os.pathsep + os.environ['PATH'])
            result = subprocess.run(['sh', str(HERE / 'feature.sh'), str(binary)], env=env,
                                    capture_output=True, text=True, timeout=15)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('forced-fixture-failure', result.stderr)
            self.assertNotIn('WEATHER-MUST-NOT-RUN', result.stdout)

if __name__ == '__main__':
    unittest.main()
