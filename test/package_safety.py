#!/usr/bin/env python3
"""Exercise package orchestration with stub builds/transport, no remote host."""
import os
import io
import tarfile
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent

class PackageSafety(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='weather-package-')
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        (self.root / 'tools').mkdir()
        shutil.copyfile(ROOT / 'tools/mkpkg-netbsd.sh', self.root / 'tools/mkpkg-netbsd.sh')
        (self.root / 'Makefile').write_text('VERSION = 0.1.0\n')
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        self.log = self.root / 'calls'
        self.env = os.environ.copy()
        self.env.update(PATH=str(self.bin) + ':' + os.environ['PATH'],
                        WEATHER_CROSS_PREFIX=str(self.bin / 'xc-'), CALLS=str(self.log))
        self.stub('make', 'mkdir -p build/pkg/stage/usr/pkg/bin\nprintf app >build/pkg/stage/usr/pkg/bin/weather\n')
        self.stub('xc-readelf', 'echo "Shared library: [libc.so.12]"\n')
        self.stub('xc-gcc', 'echo "gcc (stub) 12.5.0"\n')
        self.stub('maulctl', 'echo "$*" >>"$CALLS"\ncase "$*" in *mktemp*) echo /tmp/weather-pkg.ABCDEFGH;; esac\n')
        # Older script hardcodes ~ tool paths. Rewrite only those fixture-copy
        # paths so its pipeline can be tested before the configurable prefix fix.
        p = self.root / 'tools/mkpkg-netbsd.sh'
        p.write_text(p.read_text().replace('~/scratch/netbsd-xc/tools2/bin/powerpc--netbsd-readelf', str(self.bin / 'xc-readelf'))
                     .replace('~/scratch/netbsd-xc/tools2/bin/powerpc--netbsd-gcc', str(self.bin / 'xc-gcc')))

    def stub(self, name, body):
        p = self.bin / name
        p.write_text('#!/bin/sh\n' + body)
        p.chmod(0o700)

    def run_package(self):
        return subprocess.run(['sh', 'tools/mkpkg-netbsd.sh'], cwd=self.root,
                              env=self.env, text=True, capture_output=True)

    def test_failed_archive_never_creates_package(self):
        self.stub('tar', 'echo partial\nexit 23\n')
        result = self.run_package()
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('pkg_create', self.log.read_text() if self.log.exists() else '')
        self.assertFalse((self.root / 'build/pkg/weather-0.1.0.tgz').exists())

    def test_failed_metadata_stops_before_transport(self):
        self.stub('xc-readelf', 'exit 17\n')
        self.assertNotEqual(self.run_package().returncode, 0)
        self.assertFalse(self.log.exists())

    def test_failed_download_is_not_published(self):
        self.stub('maulctl', 'echo "$*" >>"$CALLS"\ncase "$*" in *mktemp*) echo /tmp/weather-pkg.ABCDEFGH;; *"cat "*) printf incomplete; exit 19;; esac\n')
        self.assertNotEqual(self.run_package().returncode, 0)
        self.assertFalse((self.root / 'build/pkg/weather-0.1.0.tgz').exists())

    def test_success_download_is_checked_and_published(self):
        archive = self.root / 'package.tgz'
        with tarfile.open(archive, 'w:gz') as tar:
            entry = tarfile.TarInfo('bin/weather')
            entry.size = 3
            tar.addfile(entry, io.BytesIO(b'app'))
        self.env['PACKAGE_ARCHIVE'] = str(archive)
        self.stub('maulctl', 'echo "$*" >>"$CALLS"\ncase "$*" in *mktemp*) echo /tmp/weather-pkg.ABCDEFGH;; *"cat "*) cat "$PACKAGE_ARCHIVE";; esac\n')
        result = self.run_package()
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertEqual((self.root / 'build/pkg/weather-0.1.0.tgz').read_bytes(), archive.read_bytes())
        self.assertEqual((self.root / 'build/pkg/SIZE').read_text().strip(), '3')
        self.assertFalse((self.root / 'build/pkg/stage.tar').exists())
        self.assertFalse((self.root / 'build/pkg/weather-0.1.0.tgz.part').exists())

    def test_corrupt_successful_download_is_not_published(self):
        self.stub('maulctl', 'echo "$*" >>"$CALLS"\ncase "$*" in *mktemp*) echo /tmp/weather-pkg.ABCDEFGH;; *"cat "*) printf invalid;; esac\n')
        self.assertNotEqual(self.run_package().returncode, 0)
        self.assertFalse((self.root / 'build/pkg/weather-0.1.0.tgz').exists())

    def test_upload_and_extract_failures_stop_before_packaging(self):
        for pattern in ['push nb *stage.tar*', 'nb tar *']:
            with self.subTest(pattern=pattern):
                self.log.unlink(missing_ok=True)
                self.stub('maulctl', 'echo "$*" >>"$CALLS"\ncase "$*" in *mktemp*) echo /tmp/weather-pkg.ABCDEFGH;; ' + pattern + ') exit 24;; esac\n')
                self.assertNotEqual(self.run_package().returncode, 0)
                self.assertNotIn('pkg_create', self.log.read_text())

if __name__ == '__main__':
    unittest.main()
