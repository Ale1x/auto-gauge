#!/usr/bin/env python3
"""Run the first software increment checks in disposable build directories."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(args):
    print('+ ' + ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--skip-ios-build', action='store_true',
                        help='Check shared fixtures, C host core and Swift package only')
    args = parser.parse_args()
    for executable in ('cmake', 'swift'):
        if shutil.which(executable) is None:
            raise SystemExit(f'Required development tool missing: {executable}')
    if not args.skip_ios_build and shutil.which('xcodebuild') is None:
        raise SystemExit('xcodebuild missing; use --skip-ios-build for host checks only')
    run([sys.executable, 'scripts/check_shared_contract.py'])
    with tempfile.TemporaryDirectory(prefix='auto-gauge-check-') as temp:
        scratch = Path(temp)
        run(['cmake', '-S', ROOT / 'firmware', '-B', scratch / 'firmware',
             '-DCMAKE_BUILD_TYPE=Debug'])
        run(['cmake', '--build', scratch / 'firmware'])
        run(['ctest', '--test-dir', scratch / 'firmware', '--output-on-failure'])
        run(['swift', 'test', '--package-path', ROOT / 'mobile',
             '--scratch-path', scratch / 'swift'])
        if not args.skip_ios_build:
            run(['xcodebuild', '-quiet', '-project', ROOT / 'mobile/AutoGauge.xcodeproj',
                 '-scheme', 'AutoGauge', '-configuration', 'Debug',
                 '-destination', 'generic/platform=iOS Simulator',
                 '-derivedDataPath', scratch / 'ios', 'CODE_SIGNING_ALLOWED=NO', 'build'])
    print('Software checks passed. No ESP-IDF/device, BLE, cloud or automotive validation performed.')


if __name__ == '__main__':
    main()
