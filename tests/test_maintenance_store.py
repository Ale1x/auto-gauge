"""Production maintenance/reset with behavioral SDK/NVS and sanitizers."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MaintenanceStoreTests(unittest.TestCase):
    def test_durable_reset(self):
        with tempfile.TemporaryDirectory(prefix='ag-maintenance-') as directory:
            binary = Path(directory) / 'maintenance'
            command = [shutil.which(os.environ.get('CC', 'cc')), '-std=c11', '-Wall',
                       '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
                       '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
            for path in ['firmware/ota_study/tests/boot_shim',
                         'firmware/platform_study/main', 'firmware/include']:
                command += ['-I', str(ROOT / path)]
            command += [str(ROOT / path) for path in [
                'firmware/src/connection.c',
                'firmware/platform_study/main/maintenance_store.c',
                'firmware/platform_study/tests/maintenance_driver.c']]
            subprocess.run(command + ['-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], text=True, capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('PASS: 4 durable maintenance groups', result.stdout)
