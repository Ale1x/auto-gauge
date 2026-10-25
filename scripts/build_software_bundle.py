#!/usr/bin/env python3
"""Build and package software evidence locally. No flash, signing or upload.

Uses the installed Xcode/Swift/C toolchains and pinned cached ESP-IDF. Does not
install packages, download SDKs, acquire membership or select a physical board.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def execute(args, log):
    print('+ ' + ' '.join(str(arg) for arg in args), flush=True)
    with log.open('w') as stream:
        result = subprocess.run(list(map(str, args)), cwd=ROOT, stdout=stream,
                                stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text()[-12000:], file=sys.stderr)
        raise RuntimeError(f'Check failed ({result.returncode}); log: {log}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'output/software')
    args = parser.parse_args()
    output = args.output.resolve()
    # Never replace existing evidence or allow the output to be a source folder.
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise SystemExit('Output directory must be empty; retain the previous bundle and choose another path.')
    for executable in ['cmake', 'swift', 'xcodebuild', 'git', 'sh']:
        if shutil.which(executable) is None:
            raise SystemExit(f'Missing installed development tool: {executable}')
    checks = []
    def check(name, command):
        execute(command, output / f'{name}.log')
        checks.append(name)
    check('host-swift', [sys.executable, 'scripts/check_software.py', '--skip-ios-build'])
    check('firmware-default', ['sh', 'firmware/idf/build.sh'])
    check('firmware-bench', ['sh', 'firmware/idf/build.sh', '--software-bench'])
    check('firmware-native', ['sh', 'firmware/idf/build.sh', '--native-profile'])
    sdk = Path(os.environ.get('AG_IDF_ROOT', str(Path.home() / '.cache/auto-gauge/esp-idf-v6.1')))
    sdk_tools = Path(os.environ.get('AG_IDF_TOOLS', str(Path.home() / '.cache/auto-gauge/idf-tools-v6.1')))
    toolchain = Path(os.environ.get('AG_XTENSA_BIN', str(sdk_tools / 'tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin')))
    idf_python = Path(os.environ.get('AG_IDF_PYTHON_ENV', str(sdk_tools / 'python_env/idf6.1_py3.14_env'))) / 'bin/python'
    for name, directory in [('default', 'build'), ('bench', 'build-bench'), ('native', 'build-native')]:
        check(f'firmware-{name}-report', [sys.executable, 'firmware/tools/report_device_build.py',
              '--build', ROOT / f'firmware/idf/{directory}', '--sdk', sdk,
              '--toolchain-bin', toolchain, '--idf-python', idf_python,
              '--output', output / f'firmware-{name}-report.json'])
    for name in ['ble', 'ota', 'platform']:
        check(f'{name}-sdk', ['sh', f'firmware/{name}_study/build.sh'])
        check(f'{name}-report', [sys.executable, f'firmware/{name}_study/report_build.py'])

    with tempfile.TemporaryDirectory(prefix='ag-delivery-ios-') as temporary:
        for name, destination in [('ios-simulator', 'generic/platform=iOS Simulator'),
                                  ('ios-device-unsigned', 'generic/platform=iOS')]:
            derived = Path(temporary) / name
            check(name, ['xcodebuild', '-quiet', '-project', 'mobile/AutoGauge.xcodeproj',
                         '-scheme', 'AutoGauge', '-configuration', 'Debug',
                         '-destination', destination, '-derivedDataPath', derived,
                         'CODE_SIGNING_ALLOWED=NO', 'build'])
            products = list((derived / 'Build/Products').glob('Debug-*/AutoGauge.app'))
            if len(products) != 1:
                raise RuntimeError(f'Expected one app product for {name}, found {len(products)}')
            shutil.copytree(products[0], output / name / 'AutoGauge.app')

    builds = {
        'firmware-default': ROOT / 'firmware/idf/build',
        'firmware-bench': ROOT / 'firmware/idf/build-bench',
        'firmware-native': ROOT / 'firmware/idf/build-native',
        **{f'{name}-compile-study': ROOT / f'firmware/{name}_study/build'
           for name in ['ble', 'ota', 'platform']},
    }
    for name, build in builds.items():
        destination = output / name
        destination.mkdir()
        project = json.loads((build / 'project_description.json').read_text())['project_name']
        images = [build / f'{project}.bin', build / f'{project}.elf']
        images += [build / 'bootloader/bootloader.bin', build / 'partition_table/partition-table.bin']
        if not images or any(not file.is_file() for file in images):
            raise RuntimeError(f'Missing build artifacts: {build}')
        for source in images:
            relative = source.relative_to(build)
            target = destination / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
        shutil.copy2(build / 'sdkconfig', destination / 'sdkconfig')

    # Exact source content for the software being delivered, including new files.
    names = subprocess.check_output(['git', 'ls-files', '--cached', '--others',
                                     '--exclude-standard', '-z'], cwd=ROOT).decode().split('\0')
    roots = {'firmware', 'mobile', 'shared', 'scripts', 'tests', 'docs'}
    suffixes = {'.c', '.h', '.swift', '.py', '.sh', '.json', '.md', '.csv', '.txt',
                '.plist', '.entitlements', '.pbxproj', '.xcscheme'}
    source_hashes = {}
    for name in sorted(set(names)):
        path = ROOT / name
        if (name and Path(name).parts[0] in roots and path.is_file()
                and (path.suffix in suffixes or path.name in
                     {'CMakeLists.txt', 'Kconfig.projbuild', 'sdkconfig.defaults', 'sdkconfig.bench.defaults', 'sdkconfig.native.defaults'})):
            source_hashes[name] = digest(path)
    (output / 'README.txt').write_text(
        'Auto Gauge: local software build evidence.\n'
        'Read manifest.json and the per-check logs. No flash or hardware access was performed.\n'
        'Default firmware is blocked; bench is an explicit volatile simulation.\n'
        'These binaries are not selected for the two unidentified ESP32 boards.\n'
        'iOS products are unsigned development builds. No purchase, upload or cloud test.\n')
    artifacts = {str(path.relative_to(output)): {'bytes': path.stat().st_size, 'sha256': digest(path)}
                 for path in sorted(output.rglob('*')) if path.is_file()}
    manifest = {
        'schema_version': 1, 'status': 'LOCAL_SOFTWARE_BUILD_EVIDENCE_NOT_HARDWARE_QUALIFIED',
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'base_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'source_tree_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT)),
        'checks_passed': checks, 'source_sha256': source_hashes, 'artifacts': artifacts,
        'compiler_target': 'esp32s3', 'physical_board_identified': False,
        'profiles': {'default': 'Blocked until a qualified local board profile is supplied',
                     'bench': 'Volatile simulated CAN/storage/display, no physical IO',
                     'native': 'Unified real SDK adapters; absent local board profile refuses before IO/radio',
                     'studies': 'Separate SDK adapter compile/link evidence with inert mains'},
        'actions': {'device_access': False, 'flash': False, 'efuse': False,
                    'signing': False, 'upload': False, 'cloud_live': False},
        'limitations': ['Unsigned iOS apps require the appropriate development/distribution setup before installation',
                        'Compiler target is not identification or approval of either user ESP32 board',
                        'Production trust, board pins/power, native qualification and physical timing remain external qualification',
                        'No claim of all standard PID semantic coverage or automotive product readiness'],
    }
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Bundle complete: {output / "manifest.json"}', flush=True)


if __name__ == '__main__':
    main()
