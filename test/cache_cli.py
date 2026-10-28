#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""Offline CLI cache behavior, using an independent record encoder and no service contact."""
import http.server
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import zlib

BIN = str(Path(sys.argv.pop(1)).resolve())


from cache_fixture import record


class CacheCLI(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="weather-cache-cli-")
        self.addCleanup(self.directory.cleanup)
        self.rc = Path(self.directory.name) / "rc"
        self.cache = Path(str(self.rc) + ".cache")
        self.rc.write_text("providers=met-no\ncache=on\nattempts=1\n")

    def run_cli(self, *args):
        return subprocess.run([BIN, "-c", str(self.rc), "-1", *args],
                              capture_output=True, text=True, timeout=10)

    def test_unexpired_met_reused_without_contact_or_network(self):
        data=record()
        struct.pack_into(">Q",data,144,int(time.time())+3600)
        struct.pack_into(">I",data,220,zlib.crc32(data[:220]))
        self.cache.write_bytes(data)
        result=self.run_cli()
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn("65.0F",result.stdout)
        self.assertIn("met-no",result.stdout)
        self.assertEqual(self.cache.read_bytes(),data)

    def test_cache_off_removes_saved_forecast(self):
        self.cache.write_bytes(record())
        self.rc.write_text("providers=met-no\ncache=off\nattempts=1\n")
        result = self.run_cli()
        self.assertEqual(result.returncode, 78)
        self.assertEqual(result.stdout, "")
        self.assertFalse(self.cache.exists())
        self.assertIn("forecast saving", result.stderr)

    def test_removal_failure_is_reported_and_saving_stays_off(self):
        self.cache.mkdir()
        self.rc.write_text("providers=met-no\ncache=off\nattempts=1\n")
        result = self.run_cli()
        self.assertEqual(result.stdout, "")
        self.assertIn("cannot remove saved forecast", result.stderr)
        self.assertIn("forecast saving", result.stderr)
        self.assertTrue(self.cache.is_dir())

    def test_plain_http_fixture_cannot_poison_cache(self):
        self.rc.write_text("providers=open-meteo\ncache=on\nattempts=1\n")
        original = record(source=1, providers=b"open-meteo")
        self.cache.write_bytes(original)
        body = (Path(__file__).parent / "fixtures/openmeteo-ok.json").read_bytes()

        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                self.send_response(200)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, *_):
                pass

        server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            result = self.run_cli("-H", f"127.0.0.1:{server.server_port}")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotIn("STALE", result.stdout)
            self.assertEqual(self.cache.read_bytes(), original)
        finally:
            server.shutdown()
            thread.join()
            server.server_close()


if __name__ == "__main__":
    unittest.main()
