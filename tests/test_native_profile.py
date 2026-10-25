"""Production native composition + bridge/core, behavioral peripheral adapters.

No radio/flash/crypto SDK operations run. Startup/refusal and application routing
are tested against unchanged production C; adapters are explicit local doubles.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeProfileTests(unittest.TestCase):
    def test_production_composition_startup_and_owner(self):
        with tempfile.TemporaryDirectory(prefix='ag-native-profile-') as directory:
            work = Path(directory)
            build = work / 'core'
            subprocess.run(['cmake', '-S', str(ROOT / 'firmware'), '-B', str(build),
                            '-DAG_SANITIZERS=ON'], check=True, capture_output=True)
            subprocess.run(['cmake', '--build', str(build), '--target', 'gauge_core',
                            '-j', '2'], check=True, capture_output=True)
            command = [shutil.which(os.environ.get('CC', 'cc')), '-std=c11', '-Wall',
                       '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
                       '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
            for path in ['firmware/ota_study/tests/boot_shim',
                         'firmware/platform_study/tests/twai_shim',
                         'firmware/ble_study/tests/shim',
                         'firmware/idf/main', 'firmware/platform_study/main',
                         'firmware/ota_study/main', 'firmware/ble_study/main',
                         'firmware/include', 'shared/c']:
                command += ['-I', str(ROOT / path)]
            command += [str(ROOT / path) for path in [
                'firmware/idf/main/native_profile.c',
                'firmware/ble_study/main/endpoint_bridge.c',
                'firmware/idf/tests/native_profile_driver.c']]
            binary = work / 'native-profile'
            subprocess.run(command + [str(build / 'libgauge_core.a'), '-lm', '-o', str(binary)], check=True)
            for scenario in ['absent', 'invalid', 'prepare-fail', 'entropy-fail',
                             'entropy-zero', 'corrupt-boot', 'interrupted',
                             'interrupted-credentials-fail', 'reset-radio-busy',
                             'queued', 'render-timeout', 'good']:
                with self.subTest(scenario=scenario):
                    result = subprocess.run([str(binary), scenario], text=True,
                                            capture_output=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn('PASS native composition scenario', result.stdout)
