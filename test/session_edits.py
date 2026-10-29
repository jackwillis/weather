#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""Run standalone session-edit scripts exclusively against disposable copies."""
import os
import pathlib
import stat
import shutil
import shlex
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
CTWM = 'menu "NetBSD"\n{\n    "File manager (xfe)" !"xfe &"\n}\nOccupyAll\n{\n    "XClock"\n}\n# keep this tail\n'
XINIT = '# start session\necho maul-belike-not-owned\nexec ctwm\n'
LINE = '/usr/pkg/bin/xbelike &   # maul-belike\n'

class SessionEdits(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='belike-session-')
        self.addCleanup(self.tmp.cleanup)
        self.base = pathlib.Path(self.tmp.name)
        self.rc = self.base / 'config with spaces'

    def run_script(self, kind, action=None, extra=None):
        env = os.environ.copy()
        env['CTWMRC' if kind == 'ctwm' else 'XINITRC'] = str(self.rc)
        if extra:
            env.update(extra)
        args = ['sh', str(ROOT / 'tools' / (kind + '-belike.sh'))]
        if action:
            args.append(action)
        return subprocess.run(args, env=env, text=True, capture_output=True)

    def test_roundtrip_atomic_modes_and_backup(self):
        for kind, original in [('ctwm', CTWM), ('xinitrc', XINIT)]:
            with self.subTest(kind=kind):
                self.rc.write_text(original)
                self.rc.chmod(0o640)
                backup = pathlib.Path(str(self.rc) + '.pre-belike')
                backup.unlink(missing_ok=True)
                with self.rc.open() as old:
                    result = self.run_script(kind)
                    self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
                    self.assertNotEqual(os.fstat(old.fileno()).st_ino, self.rc.stat().st_ino)
                    self.assertEqual(old.read(), original)
                self.assertEqual(stat.S_IMODE(self.rc.stat().st_mode), 0o640)
                self.assertEqual(backup.read_text(), original)
                installed = self.rc.read_bytes()
                self.assertEqual(self.run_script(kind).returncode, 0)
                self.assertEqual(self.rc.read_bytes(), installed)
                self.assertEqual(backup.read_text(), original)
                self.assertEqual(self.run_script(kind, 'undo').returncode, 0)
                self.assertEqual(self.rc.read_text(), original)
                inode = self.rc.stat().st_ino
                self.assertEqual(self.run_script(kind, 'undo').returncode, 0)
                self.assertEqual(self.rc.stat().st_ino, inode)

    def test_malformed_blocks_leave_entire_file_unchanged(self):
        bodies = [
            '# BEGIN maul-belike\n"XBelike"\n',
            '# END maul-belike\n',
            '# BEGIN maul-belike\n# BEGIN maul-belike\n"XBelike"\n# END maul-belike\n# END maul-belike\n',
            '# BEGIN maul-belike\nuser_setting = keep\n# END maul-belike\n',
        ]
        for body in bodies:
            for action in ['install', 'undo']:
                with self.subTest(body=body, action=action):
                    text = CTWM + body + '# must survive\n'
                    self.rc.write_text(text)
                    self.assertNotEqual(self.run_script('ctwm', action).returncode, 0)
                    self.assertEqual(self.rc.read_text(), text)

    def test_xinitrc_undo_removes_only_exact_owned_line(self):
        original = '# maul-belike notes\necho maul-belike\nexec ctwm\n'
        self.rc.write_text(original + LINE)
        self.assertEqual(self.run_script('xinitrc', 'undo').returncode, 0)
        self.assertEqual(self.rc.read_text(), original)

    def test_only_owned_line_can_be_removed_to_empty(self):
        self.rc.write_text(LINE)
        self.assertEqual(self.run_script('xinitrc', 'undo').returncode, 0)
        self.assertEqual(self.rc.read_bytes(), b'')

    def test_rename_failure_keeps_original_and_cleans_temp(self):
        stub = self.base / 'bin'
        stub.mkdir()
        (stub / 'mv').write_text('#!/bin/sh\nexit 29\n')
        (stub / 'mv').chmod(0o700)
        for kind, text in [('ctwm', CTWM), ('xinitrc', XINIT)]:
            with self.subTest(kind=kind):
                self.rc.write_text(text)
                result = self.run_script(kind, extra={'PATH': str(stub) + ':' + os.environ['PATH']})
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(self.rc.read_text(), text)
                self.assertEqual(list(self.base.glob(self.rc.name + '.belike.*')), [])

    def test_unsafe_config_or_backup_links_are_refused(self):
        target = self.base / 'untouched'
        target.write_text(XINIT)
        self.rc.symlink_to(target)
        self.assertNotEqual(self.run_script('xinitrc').returncode, 0)
        self.assertEqual(target.read_text(), XINIT)
        self.rc.unlink()
        self.rc.write_text(XINIT)
        backup = pathlib.Path(str(self.rc) + '.pre-belike')
        backup.symlink_to(target)
        self.assertNotEqual(self.run_script('xinitrc').returncode, 0)
        self.assertEqual(target.read_text(), XINIT)
        self.assertEqual(self.rc.read_text(), XINIT)

    def test_hardlinks_and_missing_anchors_are_refused(self):
        for kind in ['ctwm', 'xinitrc']:
            with self.subTest(kind=kind):
                self.rc.write_text('# unrelated config\n')
                self.assertNotEqual(self.run_script(kind).returncode, 0)
                self.assertEqual(self.rc.read_text(), '# unrelated config\n')
                self.assertFalse(pathlib.Path(str(self.rc) + '.pre-belike').exists())
                link = self.base / 'other-name'
                os.link(self.rc, link)
                self.assertNotEqual(self.run_script(kind, 'undo').returncode, 0)
                self.assertEqual(link.read_text(), '# unrelated config\n')
                link.unlink()

    def test_predictable_old_new_path_is_never_touched(self):
        victim = self.base / 'victim'
        victim.write_text('untouched\n')
        pathlib.Path(str(self.rc) + '.new').symlink_to(victim)
        self.rc.write_text(CTWM)
        self.assertEqual(self.run_script('ctwm').returncode, 0)
        self.assertEqual(victim.read_text(), 'untouched\n')
        self.assertEqual(list(self.base.glob(self.rc.name + '.belike.*')), [])

    def test_change_after_validation_is_refused_without_losing_it(self):
        self.rc.write_text(CTWM)
        self.assertEqual(self.run_script('ctwm').returncode, 0)
        original = self.rc.read_text()
        injected = '# BEGIN maul-belike\nconcurrent edit must survive\n'
        stub = self.base / 'bin'
        stub.mkdir()
        real_awk = shlex.quote(shutil.which('awk'))
        (stub / 'awk').write_text(
            '#!/bin/sh\n' + real_awk + ' "$@"\nstatus=$?\n'
            'if [ "${2:-}" = action=undo ]; then\n'
            "    printf '# BEGIN maul-belike\\nconcurrent edit must survive\\n' >> \"$CTWMRC\"\n"
            'fi\nexit "$status"\n')
        (stub / 'awk').chmod(0o700)
        result = self.run_script('ctwm', 'undo', {'PATH': str(stub) + ':' + os.environ['PATH']})
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.rc.read_text(), original + injected)

if __name__ == '__main__':
    unittest.main()
