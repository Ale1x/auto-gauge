"""Host failure-path regression of the actual adapter, with explicit SDK doubles."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NimBLELifecycleTests(unittest.TestCase):
    def test_production_adapter_with_failure_injection(self):
        compiler = shutil.which(os.environ.get('CC', 'cc'))
        if compiler is None:
            raise RuntimeError('C compiler required for NimBLE lifecycle checks')
        with tempfile.TemporaryDirectory(prefix='ag-nimble-lifecycle-') as directory:
            executable = Path(directory)/'lifecycle_tests'
            subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-UNDEBUG',
                '-I', str(ROOT/'firmware/ble_study/tests/shim'),
                '-I', str(ROOT/'firmware/include'),
                str(ROOT/'firmware/ble_study/tests/lifecycle_driver.c'),
                '-o', str(executable)], check=True)
            result = subprocess.run([str(executable)], capture_output=True, text=True,
                check=True, timeout=30)
            self.assertIn('PASS: 10 NimBLE lifecycle groups', result.stdout)

    def test_bridge_with_real_core_and_failure_injection(self):
        compiler = shutil.which(os.environ.get('CC', 'cc'))
        if compiler is None:
            raise RuntimeError('C compiler required for NimBLE bridge checks')
        with tempfile.TemporaryDirectory(prefix='ag-nimble-bridge-') as directory:
            executable = Path(directory)/'bridge_tests'
            sources = ['firmware/ble_study/tests/bridge_driver.c',
                'firmware/ble_study/main/endpoint_bridge.c',
                'firmware/src/config.c', 'firmware/src/can_policy.c',
                'firmware/src/connection.c', 'firmware/src/admission.c',
                'firmware/src/config_store.c', 'firmware/src/flash_sim.c',
                'firmware/src/history.c', 'firmware/src/device_endpoint.c',
                'shared/c/ag_frame.c', 'shared/c/ag_stream.c',
                'shared/c/ag_rpc.c', 'shared/c/ag_config_json.c']
            subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-UNDEBUG',
                '-I', str(ROOT/'firmware/ble_study/tests/shim'),
                '-I', str(ROOT/'firmware/ble_study/main'),
                '-I', str(ROOT/'firmware/include'), '-I', str(ROOT/'shared/c'),
                *[str(ROOT/source) for source in sources], '-o', str(executable)], check=True)
            result = subprocess.run([str(executable)], capture_output=True, text=True,
                timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
            self.assertIn('PASS: 5 NimBLE bridge/core groups', result.stdout)


if __name__ == '__main__':
    unittest.main()
