#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""Explore OS 9 timing samples. This is not a NIST estimator or a seed collector."""
import argparse
from collections import Counter
import csv
import io
import json
import math
from pathlib import Path
import re

COLUMNS = ("index", "phase", "ticks", "us", "call_us", "wait_us", "delay_us", "disk_us",
           "event_kind", "event_ticks", "mouse_x", "mouse_y")
MAX_BYTES = 4 * 1024 * 1024
MAX_ROWS = 8192
UINT64_MAX = (1 << 64) - 1


def load_samples(stream):
    text = stream.read(MAX_BYTES + 1)
    if len(text) > MAX_BYTES:
        raise ValueError("sample file exceeds the diagnostic limit")
    lines = text.splitlines()
    if len(lines) < 4 or lines[0] != "# WeatherEntropyProbe v1":
        raise ValueError("missing or unsupported probe header")
    footer = re.fullmatch(r"# complete=1 samples=([0-9]+)", lines[-1])
    if not footer:
        raise ValueError("capture is incomplete or has trailing data")
    if len(lines) > MAX_ROWS + 3 or any(len(line) > 512 for line in lines):
        raise ValueError("too many samples or oversized row")
    reader = csv.reader(io.StringIO("\n".join(lines[1:-1])), strict=True)
    try:
        if tuple(next(reader)) != COLUMNS:
            raise ValueError("unexpected sample columns")
        result = []
        for fields in reader:
            if len(fields) != len(COLUMNS):
                raise ValueError("wrong field count")
            if any(not re.fullmatch(r"-?[0-9]{1,20}", field) for field in fields):
                raise ValueError("invalid integer field")
            row = dict(zip(COLUMNS, map(int, fields)))
            if row["index"] != len(result) or row["phase"] not in (0, 1):
                raise ValueError("nonconsecutive index or invalid phase")
            if any(not 0 <= row[name] <= UINT64_MAX for name in COLUMNS[2:8]):
                raise ValueError("timing outside unsigned 64-bit range")
            if row["ticks"] > 0xffffffff or not 0 <= row["event_ticks"] <= 0xffffffff:
                raise ValueError("tick count outside unsigned 32-bit range")
            if not 0 <= row["event_kind"] <= 23:
                raise ValueError("unknown event kind")
            if any(not -32768 <= row[name] <= 32767 for name in ("mouse_x", "mouse_y")):
                raise ValueError("mouse coordinate outside signed 16-bit range")
            result.append(row)
    except (csv.Error, StopIteration) as error:
        raise ValueError("malformed sample CSV") from error
    if not result or len(result) != int(footer[1]):
        raise ValueError("capture count does not match footer")
    return result


def describe(values):
    if not values:
        return {"count": 0}
    counts = Counter(values)
    most = counts.most_common(1)[0][1]
    mean = sum(values) / len(values)
    variance_sum = sum((value - mean) ** 2 for value in values)
    correlation = None
    if variance_sum and len(values) > 1:
        correlation = sum((a - mean) * (b - mean) for a, b in zip(values, values[1:])) / variance_sum
    longest = current = 1
    for a, b in zip(values, values[1:]):
        current = current + 1 if a == b else 1
        longest = max(longest, current)
    return {"count": len(values), "unique": len(counts), "minimum": min(values), "maximum": max(values),
            "most_common_fraction": most / len(values), "empirical_marginal_bits": -math.log2(most / len(values)),
            "zero_fraction": counts[0] / len(values), "longest_identical_run": longest,
            "lag1_autocorrelation": correlation}


def summarize(rows, environment, run):
    report = {"environment": environment, "run": run, "samples": len(rows),
              "assessment": "exploratory_only", "credited_entropy_bits": 0,
              "clock_regressions": sum(b["us"] < a["us"] for a, b in zip(rows, rows[1:])), "phases": {}}
    for phase, label in ((0, "idle"), (1, "interaction")):
        selected = [row for row in rows if row["phase"] == phase]
        sources = {name: [row[name] for row in selected if row[name] != UINT64_MAX]
                   for name in ("call_us", "wait_us", "delay_us", "disk_us")}
        sources["event_gap_us"] = []
        sources["event_tick_gap"] = []
        sources["mouse_gap_us"] = []
        previous_event = previous_event_ticks = previous_mouse = previous_position = None
        for row in selected:
            if row["event_kind"] in (1, 2, 3, 4, 5):
                if previous_event is not None and row["us"] >= previous_event:
                    sources["event_gap_us"].append(row["us"] - previous_event)
                if previous_event_ticks is not None:
                    sources["event_tick_gap"].append((row["event_ticks"] - previous_event_ticks) & 0xffffffff)
                previous_event = row["us"]
                previous_event_ticks = row["event_ticks"]
            position = (row["mouse_x"], row["mouse_y"])
            if position != previous_position:
                if previous_mouse is not None and row["us"] >= previous_mouse:
                    sources["mouse_gap_us"].append(row["us"] - previous_mouse)
                previous_mouse, previous_position = row["us"], position
        phase_report = {name: describe(values) for name, values in sources.items()}
        duration = (selected[-1]["us"] - selected[0]["us"]) / 1000000 if len(selected) > 1 else 0
        phase_report["wall_span_seconds"] = max(0, duration)
        phase_report["invalid_latency_samples"] = {name: sum(row[name] == UINT64_MAX for row in selected)
                                                    for name in ("call_us", "wait_us", "delay_us", "disk_us")}
        for name, values in sources.items():
            phase_report[name]["observations_per_second"] = len(values) / duration if duration > 0 else None
        report["phases"][label] = phase_report
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--environment", required=True, choices=("classic", "native-os9"))
    parser.add_argument("--run", required=True, choices=("idle", "manual", "scripted"))
    args = parser.parse_args()
    try:
        with args.capture.open(encoding="ascii", errors="strict") as source:
            report = summarize(load_samples(source), args.environment, args.run)
    except (OSError, UnicodeError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps(report, indent=2, sort_keys=True, allow_nan=False))


if __name__ == "__main__":
    main()
