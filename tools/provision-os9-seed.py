#!/usr/bin/env python3
"""Generate a unique OS 9 seed import. Never overwrite a file or print seed bytes."""
import argparse
import hashlib
import os
from pathlib import Path


def provision(output):
    output = Path(output).resolve()
    checkout = Path(__file__).resolve().parents[1]
    if output == checkout or checkout in output.parents:
        raise ValueError("seed output must be outside the checkout")
    record = b"WXSEED1\n" + os.urandom(32)
    record += hashlib.sha256(record).digest()
    # Exclusive creation and owner-only access; an existing seed is never replaced.
    fd = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(record)
            handle.flush()
            os.fsync(handle.fileno())
    except BaseException:
        output.unlink(missing_ok=True)
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, help="new import path outside the checkout")
    args = parser.parse_args()
    try:
        provision(args.output)
    except (OSError, ValueError) as error:
        parser.exit(1, f"seed provision failed: {error}\n")
