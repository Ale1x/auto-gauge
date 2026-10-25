#!/usr/bin/env python3
"""Record evidence from an existing ESP32-S3 build, without touching hardware."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def run(*args):
    return subprocess.check_output(list(map(str, args)), text=True).strip()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--toolchain-bin', type=Path, required=True)
    parser.add_argument('--idf-python', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    sdk = args.sdk.resolve()
    elf = build / 'auto_gauge_safe_skeleton.elf'
    compiler = args.toolchain_bin / 'xtensa-esp32s3-elf-gcc'
    nm = args.toolchain_bin / 'xtensa-esp32s3-elf-nm'
    size = json.loads(run(args.idf_python, '-m', 'esp_idf_size', '--format', 'json2',
                          build / 'auto_gauge_safe_skeleton.map'))
    config = {}
    for line in (build / 'sdkconfig').read_text().splitlines():
        if line.startswith('CONFIG_') and '=' in line:
            key, value = line.split('=', 1)
            config[key] = json.loads(value) if value.startswith('"') else value
        elif line.startswith('# CONFIG_') and line.endswith(' is not set'):
            config[line[2:-11]] = 'n'
    keys = ['CONFIG_IDF_TARGET', 'CONFIG_ESPTOOLPY_FLASHSIZE', 'CONFIG_PARTITION_TABLE_FILENAME',
            'CONFIG_PARTITION_TABLE_SINGLE_APP', 'CONFIG_PARTITION_TABLE_TWO_OTA',
            'CONFIG_SPIRAM', 'CONFIG_BT_ENABLED', 'CONFIG_ESP_WIFI_ENABLED',
            'CONFIG_SECURE_BOOT', 'CONFIG_FLASH_ENCRYPTION_ENABLED']
    symbols = run(nm, '-g', '--defined-only', elf)
    ag_symbols = [line.split()[-1] for line in symbols.splitlines() if line.split()[-1].startswith('ag_')]
    radio_names = ['esp_wifi_init', 'esp_bt_controller_init', 'nimble_port_init']
    artifacts = {}
    for relative in ['auto_gauge_safe_skeleton.bin', 'auto_gauge_safe_skeleton.elf',
                     'bootloader/bootloader.bin', 'partition_table/partition-table.bin']:
        path = build / relative
        artifacts[relative] = dict(bytes=path.stat().st_size, sha256=digest(path))
    compiled = sorted({str(Path(item['file']).resolve().relative_to(ROOT))
                       for item in json.loads((build / 'compile_commands.json').read_text())
                       if str(Path(item['file']).resolve()).startswith(str(ROOT / 'firmware/src') + '/')})
    compiled_shared = sorted({str(Path(item['file']).resolve().relative_to(ROOT))
                       for item in json.loads((build / 'compile_commands.json').read_text())
                       if str(Path(item['file']).resolve()).startswith(str(ROOT / 'shared/c') + '/')})
    report = dict(
        generated_utc=datetime.now(timezone.utc).isoformat(),
        status='ESP32S3_BUILD_SUCCESS_NO_DEVICE_RUN',
        target='esp32s3', sdk_tag=run('git', '-C', sdk, 'describe', '--tags', '--exact-match'),
        sdk_commit=run('git', '-C', sdk, 'rev-parse', 'HEAD'),
        compiler=run(compiler, '--version').splitlines()[0],
        python=run(args.idf_python, '--version'),
        configuration={key: config.get(key, 'UNSET') for key in keys},
        artifacts=artifacts, memory=size,
        compiled_core_sources=compiled, compiled_shared_sources=compiled_shared, linked_ag_symbols=ag_symbols,
        compiled_source_sha256={p:digest(ROOT/p) for p in compiled+compiled_shared},
        radio_init_symbols_present={name: any(line.split()[-1] == name for line in symbols.splitlines())
                                    for name in radio_names},
        main_sha256=digest(ROOT / 'firmware/idf/main/app_main.c'),
        hal_sha256=digest(ROOT / 'firmware/idf/main/hal_unimplemented.c'),
        actions=dict(build='EXECUTED', size='EXECUTED', flash='NOT_EXECUTED',
                     efuse='NOT_EXECUTED', radio_or_hardware_test='NOT_EXECUTED'),
        limitations=['Inert skeleton; many compiled core functions discarded by linker GC',
                     'No panel driver/assets, operational BLE/TWAI, persistent logging or OTA image',
                     'Factory single-app 2MB development configuration; not the candidate N16R8 memory map',
                     'Secure boot/encryption not provisioned; binary is not a production/recovery release',
                     'SDK Wi-Fi capability flag is not proof of runtime radio initialization',
                     'No OTA/PSRAM sizing or hardware compatibility proven'])
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"Wrote {args.output}: bin={artifacts['auto_gauge_safe_skeleton.bin']['bytes']} bytes, image={size['total_size']} bytes")


if __name__ == '__main__':
    main()
