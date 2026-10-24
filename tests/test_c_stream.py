import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CStreamTests(unittest.TestCase):
    def test_bounded_stream_with_sanitizers(self):
        compiler = shutil.which(os.environ.get('CC', 'cc'))
        if compiler is None: raise RuntimeError('C compiler required for stream checks')
        with tempfile.TemporaryDirectory(prefix='ag-stream-') as directory:
            executable = Path(directory)/'stream_tests'
            subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-UNDEBUG',
                '-I', str(ROOT/'shared/c'), str(ROOT/'shared/c/ag_frame.c'),
                str(ROOT/'shared/c/ag_stream.c'), str(ROOT/'tests/stream_driver.c'),
                '-o', str(executable)], check=True)
            result = subprocess.run([str(executable)], capture_output=True, text=True, check=True)
            self.assertIn('PASS: 6 stream groups', result.stdout)


if __name__ == '__main__': unittest.main()
