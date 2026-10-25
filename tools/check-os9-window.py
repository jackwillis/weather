#!/usr/bin/env python3
"""Drive only WeatherUITest's explicit marker protocol; capture native screenshots.

Build with WEATHER_BUILD_DIAGNOSTICS=ON and deliver WeatherUITest first. Never
answers a system dialog or changes an owner's settings. Output stays local.
"""
import argparse
import base64
from pathlib import Path
import subprocess
import time

MAULCTL = str(Path.home() / ".local/bin/maulctl")
REMOTE = "/Volumes/MacOS9Lives/weather-dev"


def remote(command):
    return subprocess.run([MAULCTL, "osx", command], check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          timeout=45).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--capture", nargs="*", type=int, default=list(range(12)),
                        help="stages to capture (all by default); assertions always run")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    # Only diagnostic-owned markers/logs. Never touch production config or seeds.
    remote(f"rm -f {REMOTE}/weather-ui.ready {REMOTE}/weather-ui.advance "
           f"{REMOTE}/weather-ui-test.log; open {REMOTE}/WeatherUITest")
    names = ["clear-day", "clear-night", "cloud", "rain", "snow", "storm",
             "fog", "about", "metric", "stale-error", "after-yield", "live"]
    for stage, name in enumerate(names):
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            ready = remote(f"cat {REMOTE}/weather-ui.ready 2>/dev/null || true").strip()
            if ready == str(stage).encode():
                break
            time.sleep(1)
        else:
            log = remote(f"cat {REMOTE}/weather-ui-test.log 2>/dev/null || true")
            (args.output / "weather-ui-test.log").write_bytes(log)
            raise RuntimeError(f"UI stage {stage} did not become ready; see saved log")
        if stage in args.capture:
            shot = remote("/usr/sbin/screencapture -x /tmp/weather-ui-shot.png; "
                          "openssl base64 < /tmp/weather-ui-shot.png")
            (args.output / f"{stage:02d}-{name}.png").write_bytes(base64.b64decode(shot))
            print(f"captured {stage}: {name}", flush=True)
        remote(f"printf '{stage}\\n' > {REMOTE}/weather-ui.advance.tmp; "
               f"mv -f {REMOTE}/weather-ui.advance.tmp {REMOTE}/weather-ui.advance")
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        log = remote(f"cat {REMOTE}/weather-ui-test.log")
        if b"finished:" in log:
            (args.output / "weather-ui-test.log").write_bytes(log)
            if b"FAIL" in log or b"finished: 0 failures; rc=0" not in log:
                raise RuntimeError("native window scenario failed; see saved log")
            print("native window scenario passed", flush=True)
            return
        time.sleep(1)
    raise RuntimeError("native window did not terminate after Quit")


if __name__ == "__main__":
    main()
