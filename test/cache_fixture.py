# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""Independent cache record fixture; never contains TLS seed material."""
from pathlib import Path
import struct
import time
import zlib

def record(stamp=None, latitude=430747, metric=False, source=2, providers=b"met-no", expires=0):
    data = bytearray(224)
    data[:8] = b"WXFCACH3"
    struct.pack_into(">iiBB", data, 8, latitude, -893841, int(metric), source)
    struct.pack_into(">Q", data, 24, int(time.time()) if stamp is None else stamp)
    struct.pack_into(">6i", data, 32, 185 if metric else 650, 67, 80, 225, 0, 1)
    observation = b"2026-10-25T12:00"
    data[56:56 + len(observation)] = observation
    data[80:80 + len(providers)] = providers
    struct.pack_into(">Q", data, 144, expires)
    struct.pack_into(">I", data, 220, zlib.crc32(data[:220]))
    return data


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 2:
        Path(sys.argv[1]).write_bytes(record(stamp=int(time.time())-int(sys.argv[2]), expires=0))
    else:
        Path(sys.argv[1]).write_bytes(record(expires=int(time.time())+3600))
