"""Production installer C with behavioral SDK and NONCRYPTO hash doubles."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class OTAInstallerTests(unittest.TestCase):
    def test_failure_paths_with_sanitizers(self):
        compiler = shutil.which(os.environ.get('CC', 'cc'))
        if compiler is None:
            raise RuntimeError('C compiler required for OTA installer checks')
        with tempfile.TemporaryDirectory(prefix='ag-ota-installer-') as directory:
            executable = Path(directory)/'installer_tests'
            subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-UNDEBUG',
                '-I', str(ROOT/'firmware/ota_study/tests/shim'),
                '-I', str(ROOT/'firmware/ota_study/main'),
                str(ROOT/'firmware/ota_study/main/ota_installer.c'),
                str(ROOT/'firmware/ota_study/tests/installer_driver.c'),
                '-o', str(executable)], check=True)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
            self.assertIn('PASS: 8 OTA installer groups', result.stdout)


if __name__ == '__main__':
    unittest.main()
