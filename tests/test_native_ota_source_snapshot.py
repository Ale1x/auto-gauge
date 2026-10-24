"""The optional native check must never attribute cache edits to pinned commits."""
import hashlib
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('native_ota', ROOT / 'scripts/check_native_ota_crypto.py')
NATIVE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(NATIVE)


class NativeOTASnapshotTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='ag-snapshot-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo = self.root / 'repo'
        self.repo.mkdir()
        self.git('init', '-q')
        (self.repo / '.gitignore').write_text('ignored/\n')
        (self.repo / 'input.c').write_text('committed source\n')
        self.git('add', '.')
        self.git('-c', 'user.name=Snapshot Test', '-c', 'user.email=test@example.invalid',
                 '-c', 'commit.gpgsign=false', 'commit', '-qm', 'fixture')
        self.revision = self.git('rev-parse', 'HEAD').decode().strip()

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.repo), *args], stderr=subprocess.STDOUT)

    def check(self):
        NATIVE.checked_checkout(self.repo, self.revision, include_ignored=True)

    def test_clean_pin_and_deterministic_committed_snapshot(self):
        self.check()
        first = NATIVE.committed_snapshot(self.repo, self.revision, self.root / 'first')
        second = NATIVE.committed_snapshot(self.repo, self.revision, self.root / 'second')
        self.assertEqual(first, second)
        self.assertEqual((self.root / 'first/input.c').read_text(), 'committed source\n')
        self.assertFalse((self.root / 'first/.git').exists())
        archive = self.git('archive', '--format=tar', self.revision)
        self.assertEqual(first['archive_sha256'], hashlib.sha256(archive).hexdigest())
        with self.assertRaises(RuntimeError):
            NATIVE.checked_checkout(self.repo, '0' * 40)

    def test_rejects_modified_staged_untracked_and_ignored(self):
        for kind in ['modified', 'staged', 'untracked', 'ignored']:
            with self.subTest(kind=kind):
                if kind in ('modified', 'staged'):
                    (self.repo / 'input.c').write_text('modified\n')
                    if kind == 'staged':
                        self.git('add', 'input.c')
                else:
                    path = self.repo / ('ignored/file.c' if kind == 'ignored' else 'new.c')
                    path.parent.mkdir(exist_ok=True)
                    path.write_text('untracked\n')
                with self.assertRaises(RuntimeError):
                    self.check()
                self.git('reset', '--hard', '-q', self.revision)
                self.git('clean', '-fdxq')

    def test_snapshot_cannot_copy_post_check_worktree_changes(self):
        self.check()
        (self.repo / 'input.c').write_text('raced source\n')
        (self.repo / 'injected.c').write_text('untracked\n')
        NATIVE.committed_snapshot(self.repo, self.revision, self.root / 'snapshot')
        self.assertEqual((self.root / 'snapshot/input.c').read_text(), 'committed source\n')
        self.assertFalse((self.root / 'snapshot/injected.c').exists())

    def test_rejects_symlinks_and_gitlinks(self):
        for mode, name in [('120000', 'symlink'), ('160000', 'submodule')]:
            with self.subTest(mode=mode):
                if mode == '120000':
                    (self.repo / name).symlink_to('input.c')
                    self.git('add', name)
                else:
                    self.git('update-index', '--add', '--cacheinfo', mode, self.revision, name)
                self.git('-c', 'user.name=Snapshot Test', '-c', 'user.email=test@example.invalid',
                         '-c', 'commit.gpgsign=false', 'commit', '-qm', name)
                revision = self.git('rev-parse', 'HEAD').decode().strip()
                with self.assertRaises(RuntimeError):
                    NATIVE.committed_snapshot(self.repo, revision, self.root / name)
                self.git('reset', '--hard', '-q', self.revision)
                self.git('clean', '-fdxq')

    def test_checkout_rejects_dirty_submodule(self):
        origin = self.root / 'origin'
        subprocess.run(['git', 'clone', '-q', str(self.repo), str(origin)], check=True)
        self.git('-c', 'protocol.file.allow=always', 'submodule', 'add', '-q', str(origin), 'vendor')
        self.git('-c', 'user.name=Snapshot Test', '-c', 'user.email=test@example.invalid',
                 '-c', 'commit.gpgsign=false', 'commit', '-qam', 'vendor fixture')
        self.revision = self.git('rev-parse', 'HEAD').decode().strip()
        self.check()
        (self.repo / 'vendor/input.c').write_text('dirty vendor\n')
        with self.assertRaises(RuntimeError):
            self.check()
