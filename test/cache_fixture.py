"""Independent cache record fixture; never contains TLS seed material."""
from pathlib import Path
import struct
import time
import zlib

def record(stamp=None, latitude=430747, metric=False, source=2, providers=b"met-no"):
    data = bytearray(160)
    data[:8] = b"WXFCACH1"
    struct.pack_into(">iiBB", data, 8, latitude, -893841, int(metric), source)
    struct.pack_into(">Q", data, 24, int(time.time()) if stamp is None else stamp)
    struct.pack_into(">6i", data, 32, 185 if metric else 650, 67, 80, 225, 0, 1)
    observation = b"2026-10-25T12:00"
    data[56:56 + len(observation)] = observation
    data[80:80 + len(providers)] = providers
    struct.pack_into(">I", data, 156, zlib.crc32(data[:156]))
    return data


if __name__ == "__main__":
    import sys
    Path(sys.argv[1]).write_bytes(record())
