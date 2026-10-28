#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""Exploratory measurement analysis must never turn synthetic variation into seed credit."""
import importlib.util
import io
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("entropy_analysis", Path(__file__).parents[1] / "tools/analyze-os9-entropy.py")
analysis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analysis)


def capture(values):
    rows = ["# WeatherEntropyProbe v1", ",".join(analysis.COLUMNS)]
    for i, value in enumerate(values):
        rows.append(f"{i},0,{i},{i * 20000},{value},{value},{value},{value},0,{i},0,0")
    rows.append(f"# complete=1 samples={len(values)}")
    return "\n".join(rows) + "\n"


class AnalysisTests(unittest.TestCase):
    def report(self, values):
        return analysis.summarize(analysis.load_samples(io.StringIO(capture(values))), "classic", "idle")

    def test_constant_has_no_claim_or_credit(self):
        result = self.report([17] * 20)
        source = result["phases"]["idle"]["call_us"]
        self.assertEqual(source["empirical_marginal_bits"], 0)
        self.assertIsNone(source["lag1_autocorrelation"])
        self.assertEqual(result["credited_entropy_bits"], 0)
        self.assertEqual(result["assessment"], "exploratory_only")

    def test_predictable_counter_or_periodic_data_never_earns_credit(self):
        for values in (list(range(100)), [0, 1] * 50):
            result = self.report(values)
            self.assertGreater(result["phases"]["idle"]["call_us"]["empirical_marginal_bits"], 0)
            self.assertEqual(result["credited_entropy_bits"], 0)

    def test_complete_format_and_range_checks(self):
        text = capture([1, 2, 3])
        variants = (text.replace("complete=1", "complete=0"), text.rsplit("# complete", 1)[0],
                    text.replace("samples=3", "samples=2"), text.replace("0,0,0,0,1", "9,0,0,0,1"),
                    text.replace("20000", str(1 << 65)), text.replace("v1", "v2"), text + "garbage\n")
        for value in variants:
            with self.subTest(value=value[-70:]):
                with self.assertRaises(ValueError):
                    analysis.load_samples(io.StringIO(value))

    def test_bounded_input(self):
        with self.assertRaises(ValueError):
            analysis.load_samples(io.StringIO("x" * (analysis.MAX_BYTES + 1)))

    def test_mutated_rows_fail_cleanly_or_still_have_zero_credit(self):
        original = capture([1, 2, 1, 3])
        for position in range(len(original)):
            mutated = original[:position] + "x" + original[position + 1:]
            try:
                rows = analysis.load_samples(io.StringIO(mutated))
            except ValueError:
                continue
            self.assertEqual(analysis.summarize(rows, "native-os9", "scripted")["credited_entropy_bits"], 0)


if __name__ == "__main__":
    unittest.main()
