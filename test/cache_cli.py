#!/usr/bin/env python3
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

    def test_offline_fallback_is_stale_and_preserves_failure(self):
        self.cache.write_bytes(record())
        result = self.run_cli()
        self.assertEqual(result.returncode, 78)  # Missing MET contact is a config error; no network call.
        self.assertTrue(result.stdout.startswith("STALE cached forecast"), result)
        self.assertIn("65.0F", result.stdout)
        self.assertIn("met-no", result.stdout)
        self.assertIn("requires 'contact'", result.stderr)

    def test_cache_is_opt_in(self):
        self.cache.write_bytes(record())
        self.rc.write_text("providers=met-no\nattempts=1\n")
        result = self.run_cli()
        self.assertEqual(result.returncode, 78)
        self.assertEqual(result.stdout, "")

    def test_unusable_entries_never_look_live(self):
        corrupt = record(); corrupt[32] ^= 1
        reserved = record(); reserved[144] = 1
        struct.pack_into(">I", reserved, 156, zlib.crc32(reserved[:156]))
        for data in (record(latitude=1), record(metric=True), record(stamp=int(time.time()) - 86401),
                     record(stamp=int(time.time()) + 60), record()[:-1], record() + b"x", corrupt, reserved):
            with self.subTest(data=data[:32]):
                self.cache.write_bytes(data)
                result = self.run_cli()
                self.assertEqual(result.returncode, 78)
                self.assertEqual(result.stdout, "")

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
