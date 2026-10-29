# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""User-facing Unix command/config contract; no service requests."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1] if len(sys.argv) > 1 else "build/host/belike").resolve()
if not binary.is_file():
    raise SystemExit("FAIL Unix names: belike executable missing")
version = subprocess.run([str(binary), "-V"], capture_output=True, text=True, check=True)
assert version.stdout.startswith("belike "), version.stdout
with tempfile.TemporaryDirectory(prefix="belike-names-") as directory:
    home = Path(directory)
    old = home / ".weatherrc"
    legacy = "not a valid setting\n"
    old.write_text(legacy)
    env = dict(os.environ, HOME=directory)
    result = subprocess.run([str(binary), "-n", "Rename test", "-S"], env=env,
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    current = home / ".belikerc"
    assert current.is_file(), "new rc file was not created"
    assert "Rename test" in current.read_text()
    assert old.read_text() == legacy, "legacy rc must be ignored and unchanged"
    current.write_text("name = Saved\nunits = metric\n")
    result = subprocess.run([str(binary), "-n", "Updated", "-S"], env=env,
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert "Updated" in current.read_text() and "metric" in current.read_text()
    assert old.read_text() == legacy
print("Unix names: version, new rc read/write and no legacy migration passed")
