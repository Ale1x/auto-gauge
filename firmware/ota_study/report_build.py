#!/usr/bin/env python3
"""Read compile/link evidence only. Never executes image or touches a device."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
BUILD=HERE/'build'
SDK=Path(os.environ.get('AG_IDF_ROOT',str(Path.home()/'.cache/auto-gauge/esp-idf-v6.1')))
BIN=Path(os.environ.get('AG_XTENSA_BIN',str(Path.home()/'.cache/auto-gauge/idf-tools-v6.1/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin')))
ELF=BUILD/'auto_gauge_ota_compile_study.elf'
actual=subprocess.check_output(['git','-C',str(SDK),'rev-parse','HEAD'],text=True).strip()
assert actual=='fff9895c82d744c7237be8847347bdd1b07c6643'
config=(BUILD/'sdkconfig').read_text()
settings={}
for name in ['ESPTOOLPY_FLASHSIZE','PARTITION_TABLE_TWO_OTA','BOOTLOADER_APP_ROLLBACK_ENABLE',
             'BOOTLOADER_APP_ANTI_ROLLBACK','SECURE_BOOT','FLASH_ENCRYPTION_ENABLED','BT_ENABLED','ESP_WIFI_ENABLED']:
    key='CONFIG_'+name;match=re.search(r'^'+key+r'=(.+)$',config,re.M)
    settings[key]=match.group(1) if match else 'n'
assert settings['CONFIG_PARTITION_TABLE_TWO_OTA']=='y'
assert settings['CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE']=='y'
assert settings['CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK']=='n'
assert settings['CONFIG_SECURE_BOOT']=='n' and settings['CONFIG_FLASH_ENCRYPTION_ENABLED']=='n'
assert settings['CONFIG_BT_ENABLED']=='n'
nm=subprocess.check_output([str(BIN/'xtensa-esp32s3-elf-nm'),'--defined-only',str(ELF)],text=True)
entries=['init','begin','write','finish','select','poll','cancel']
required=['ag_ota_installer_'+n for n in entries]+['esp_ota_begin','esp_ota_write','esp_ota_end',
    'esp_ota_set_boot_partition','esp_ota_abort','esp_partition_read','psa_hash_setup','psa_hash_update','psa_hash_finish',
    'ag_manifest_verify','ag_manifest_verify_hook','psa_import_key','psa_verify_hash','psa_destroy_key','psa_hash_compute',
    'ag_ota_transfer_adapter_init','ag_ota_transfer_adapter_handle','ag_ota_transfer_adapter_poll',
    'ag_ot_owner_init','ag_ot_owner_handle','ag_ot_owner_poll',
    'ag_ota_framed_adapter_init','ag_ota_framed_adapter_feed','ag_ota_framed_adapter_step',
    'ag_ota_framed_adapter_take_output','ag_ota_framed_adapter_poll','ag_ota_framed_adapter_close',
    'ag_ot_endpoint_init','ag_ot_endpoint_feed','ag_ot_endpoint_step','ag_ot_endpoint_take_output',
    'ag_ot_endpoint_poll','ag_ot_endpoint_close','ag_frame_encode','ag_frame_decode','ag_stream_feed']
for name in required:assert re.search(r'^[0-9a-fA-F]+ [Tt] '+name+r'$',nm,re.M),name
radio_names=['esp_wifi_init','esp_bt_controller_init','nimble_port_init']
radio_symbols={name:bool(re.search(r'^[0-9a-fA-F]+ [Tt] '+name+r'$',nm,re.M)) for name in radio_names}
assert not any(radio_symbols.values())
assembly=subprocess.check_output([str(BIN/'xtensa-esp32s3-elf-objdump'),'-d','--disassemble=app_main',str(ELF)],text=True)
main=[line.strip() for line in assembly.splitlines() if re.match(r'^\s*[0-9a-fA-F]+:',line)]
assert main and not any(re.search(r'\bcall\w*\b',line) for line in main)
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
artifacts=[ELF,BUILD/'auto_gauge_ota_compile_study.bin',BUILD/'partition_table/partition-table.bin']
sources=[HERE/'main/ota_installer.c',HERE/'main/ota_installer.h',HERE/'main/main.c',
         HERE/'main/manifest_verify.c',HERE/'main/manifest_verify.h',
         HERE/'main/transfer_adapter.c',HERE/'main/transfer_adapter.h',
         ROOT/'firmware/src/ota_transfer.c',ROOT/'firmware/include/ota_transfer.h',
         HERE/'main/framed_adapter.c',HERE/'main/framed_adapter.h',
         ROOT/'firmware/src/ota_framed_endpoint.c',ROOT/'firmware/include/ota_framed_endpoint.h',
         ROOT/'shared/c/ag_frame.c',ROOT/'shared/c/ag_frame.h',ROOT/'shared/c/ag_stream.c',ROOT/'shared/c/ag_stream.h',
         HERE/'main/CMakeLists.txt',HERE/'sdkconfig.defaults']
report=dict(status='COMPILE_LINK_ONLY_NOT_RUN',sdk_commit=actual,configuration=settings,
    radio_init_symbols_present=radio_symbols,
    retained_symbols=required,main_instructions=main,
    artifacts={str(p.relative_to(BUILD)):dict(bytes=p.stat().st_size,sha256=digest(p)) for p in artifacts},
    source_sha256={str(p.relative_to(ROOT)):digest(p) for p in sources},
    actions=dict(build='EXECUTED',device_execution='NOT_EXECUTED',flash='NOT_EXECUTED',
                 boot_selection='NOT_EXECUTED',efuse='NOT_EXECUTED',radio='NOT_EXECUTED'),
    limitations=['Empty app_main; linked installer APIs would erase/write/select if called in future',
        '4MB factory+two-OTA development table is not the product memory map or 2MiB log sizing',
        'Target manifest verifier and transfer adapter linked; trusted hardware/floor/key lookup and platform policy remain caller supplied',
        'Framed endpoint and payload controller linked to installer but no main or BLE/RPC route; no production key installed',
        'No health confirmation, reboot, durable release-counter transaction or recovery after ambiguous selection',
        'No Secure Boot/encryption provisioning, physical flash/brownout/wear/rollback qualification',
        'SDK Wi-Fi capability flag may be enabled; initialization symbols absent and main never invokes them',
        'SDK compile/link only: PSA hardware provider not executed; native portable crypto has separate evidence',
        'Behavioral fault-injection harness remains separate from real native RSA/SHA test'])
(HERE/'build-report.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS:7 installer +2 manifest +3 payload adapter +6 framed adapter entry points and real SDK dependencies linked; app_main calls none')
print('Image bytes:',report['artifacts']['auto_gauge_ota_compile_study.bin']['bytes'])
