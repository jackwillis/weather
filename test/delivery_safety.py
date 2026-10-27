#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""Drive delivery with fake disk tools and a disposable destination only."""
import os
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent

class DeliverySafety(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='weather-delivery-')
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.dest = self.root / 'destination'
        self.source = self.root / 'image'
        self.state = self.root / 'state'
        self.bin = self.root / 'bin'
        for p in [self.dest, self.source, self.state, self.bin]:
            p.mkdir()
        self.app = 'Weather Test'
        (self.source / self.app).write_text('new application')
        (self.dest / self.app).write_text('old application')
        self.done = self.state / 'deliver.done'
        self.log = self.root / 'commands'
        self.env = os.environ.copy()
        self.env.update(PATH=str(self.bin) + ':' + os.environ['PATH'],
                        WEATHER_DELIVER_DEST=str(self.dest), WEATHER_DELIVER_STATE=str(self.state),
                        SOURCE_DIR=str(self.source), CALLS=str(self.log), FAIL_STEP='')
        self.stub('hdiutil', 'echo "hdiutil $*" >>"$CALLS"\nif [ "$1" = attach ]; then echo /dev/disk99; fi\n[ "$FAIL_STEP" != "$1" ]\n')
        self.stub('diskutil', 'echo "diskutil $*" >>"$CALLS"\n[ "$FAIL_STEP" != "$1" ] || exit 21\nif [ "$1" = info ]; then echo "   Mount Point: $SOURCE_DIR"; fi\n')
        self.stub('ditto', 'echo ditto >>"$CALLS"\nif [ "$FAIL_STEP" = copy ]; then printf partial >"$3"; exit 22; fi\ncp "$2" "$3"\n')
        text = (ROOT / 'tools/deliver-os9.sh').read_text()
        # Repoint only defaults in the old fixture-copy script before testing
        # the new explicit destination/state overrides. Never touch /Volumes.
        text = text.replace('dest=/Volumes/MacOS9Lives/weather-dev', 'dest=' + str(self.dest))
        text = text.replace('/tmp/deliver.done', str(self.done)).replace('/tmp/deliver.log', str(self.state / 'deliver.log'))
        self.script = self.root / 'deliver.sh'
        self.script.write_text(text)

    def stub(self, name, body):
        p = self.bin / name
        p.write_text('#!/bin/sh\n' + body)
        p.chmod(0o700)

    def run_delivery(self, failure=''):
        self.done.write_text('stale success')
        env = dict(self.env, FAIL_STEP=failure)
        return subprocess.run(['sh', str(self.script), 'fake.dsk', self.app],
                              env=env, text=True, capture_output=True)

    def test_success_and_space_in_application_name(self):
        result = self.run_delivery()
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertTrue(self.done.exists())
        self.assertEqual((self.dest / self.app).read_text(), 'new application')
        self.assertIn('hdiutil detach', self.log.read_text())

    def test_native_step_failures_do_not_mark_success(self):
        for failure in ['attach', 'mount', 'info', 'copy', 'detach']:
            with self.subTest(failure=failure):
                result = self.run_delivery(failure)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(self.done.exists())
                self.assertEqual((self.dest / self.app).read_text(), 'old application')

    def test_missing_source_keeps_destination_and_no_success(self):
        (self.source / self.app).unlink()
        self.assertNotEqual(self.run_delivery().returncode, 0)
        self.assertFalse(self.done.exists())
        self.assertEqual((self.dest / self.app).read_text(), 'old application')

    def test_failed_copy_keeps_old_application_and_detaches(self):
        self.assertNotEqual(self.run_delivery('copy').returncode, 0)
        self.assertEqual((self.dest / self.app).read_text(), 'old application')
        self.assertIn('hdiutil detach', self.log.read_text())
        self.assertEqual(list(self.dest.glob('.weather-deliver.*')), [])

    def test_missing_device_or_volume_path_is_not_success(self):
        self.stub('hdiutil', 'echo "hdiutil $*" >>"$CALLS"\necho no-device\n')
        self.assertNotEqual(self.run_delivery().returncode, 0)
        self.assertFalse(self.done.exists())
        self.stub('hdiutil', 'if [ "$1" = attach ]; then echo /dev/disk99; fi\n')
        self.stub('diskutil', 'exit 0\n')
        self.assertNotEqual(self.run_delivery().returncode, 0)
        self.assertFalse(self.done.exists())

    def test_bad_application_name_clears_stale_marker_without_native_commands(self):
        self.app = '../outside'
        self.assertNotEqual(self.run_delivery().returncode, 0)
        self.assertFalse(self.done.exists())
        self.assertFalse(self.log.exists())

if __name__ == '__main__':
    unittest.main()
