#!/usr/bin/env python3
"""Provisioning checks use temporary files and never display random material."""
import hashlib
import importlib.util
import stat
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("seed_provision", ROOT / "tools/provision-os9-seed.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class ProvisionTests(unittest.TestCase):
    def test_format_unique_permissions_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            a, b = Path(directory) / "a", Path(directory) / "b"
            MODULE.provision(a)
            MODULE.provision(b)
            data = a.read_bytes()
            self.assertTrue(len(data) == 72 and data[:8] == b"WXSEED1\n")
            self.assertTrue(data[40:] == hashlib.sha256(data[:40]).digest())
            self.assertTrue(data != b.read_bytes())
            self.assertEqual(stat.S_IMODE(a.stat().st_mode), 0o600)
            with self.assertRaises(FileExistsError):
                MODULE.provision(a)
            self.assertTrue(a.read_bytes() == data)

    def test_checkout_is_rejected(self):
        with self.assertRaises(ValueError):
            MODULE.provision(ROOT / "weather-seed.import")
        self.assertFalse((ROOT / "weather-seed.import").exists())


if __name__ == "__main__":
    unittest.main()
