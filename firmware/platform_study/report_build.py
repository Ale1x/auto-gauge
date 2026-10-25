#!/usr/bin/env python3
"""Inspect compile/link evidence only. Never reads/writes a physical device."""
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
SDK_COMMIT='fff9895c82d744c7237be8847347bdd1b07c6643'
ELF=BUILD/'auto_gauge_platform_compile_study.elf'
revision=subprocess.check_output(['git','-C',str(SDK),'rev-parse','HEAD'],text=True).strip()
assert revision==SDK_COMMIT
assert not subprocess.check_output(['git','-C',str(SDK),'status','--porcelain=v1','--untracked-files=all','--ignore-submodules=none'])
config=(BUILD/'sdkconfig').read_text()
assert 'CONFIG_PARTITION_TABLE_CUSTOM=y' in config
assert 'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"' in config
assert 'CONFIG_ESPTOOLPY_FLASHSIZE="4MB"' in config
assert 'CONFIG_BT_ENABLED=y' not in config
assert 'CONFIG_SECURE_BOOT=y' not in config and 'CONFIG_FLASH_ENCRYPTION_ENABLED=y' not in config
nm=subprocess.check_output([str(BIN/'xtensa-esp32s3-elf-nm'),'--defined-only',str(ELF)],text=True)
required=['ag_partition_store_bind','ag_config_store_open','ag_config_store_restore','ag_config_store_commit',
    'ag_config_store_get','ag_config_record_encode','ag_journal_open','ag_journal_bootstrap','ag_journal_begin_session',
    'ag_journal_append','ag_journal_window','ag_journal_acknowledge','esp_partition_read','esp_partition_write',
    'esp_partition_erase_range','esp_partition_find_err','esp_partition_verify','esp_flash_get_size',
    'ag_twai_port_init','ag_twai_port_start','ag_twai_port_send_discovery','ag_twai_port_send_runtime',
    'ag_twai_port_send_flow_control','ag_twai_port_poll','ag_twai_port_close',
    'twai_new_node_onchip','twai_node_enable','twai_node_transmit','twai_node_disable','twai_node_delete']
for name in required:assert re.search(r'^[0-9a-fA-F]+ [Tt] '+name+r'$',nm,re.M),name
radio=['esp_wifi_init','esp_bt_controller_init','nimble_port_init']
assert not any(re.search(r'^[0-9a-fA-F]+ [Tt] '+name+r'$',nm,re.M) for name in radio)
assembly=subprocess.check_output([str(BIN/'xtensa-esp32s3-elf-objdump'),'-d','--disassemble=app_main',str(ELF)],text=True)
main=[line.strip() for line in assembly.splitlines() if re.match(r'^\s*[0-9a-fA-F]+:',line)]
assert main and not any(re.search(r'\bcall\w*\b',line) for line in main)
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
artifacts=[ELF,BUILD/'auto_gauge_platform_compile_study.bin',BUILD/'partition_table/partition-table.bin']
sources=[HERE/'main/twai_port.c',HERE/'main/twai_port.h',HERE/'main/partition_io.c',HERE/'main/partition_io.h',HERE/'main/main.c',HERE/'main/CMakeLists.txt',
    HERE/'sdkconfig.defaults',HERE/'partitions.csv',ROOT/'firmware/src/config.c',ROOT/'firmware/src/config_store.c',
    ROOT/'firmware/src/history.c',ROOT/'firmware/src/journal_history.c',ROOT/'firmware/include/ag_config_store.h',
    ROOT/'firmware/include/ag_journal_history.h',
    ROOT/'firmware/src/runtime.c',ROOT/'firmware/src/discovery.c',ROOT/'firmware/src/can_policy.c',
    ROOT/'firmware/src/obd_decode.c',ROOT/'firmware/src/scheduler.c',ROOT/'firmware/src/isotp_rx.c',
    ROOT/'firmware/include/ag_runtime.h',ROOT/'firmware/include/ag_discovery.h',ROOT/'firmware/include/ag_diagnostics.h']
vendor=['components/esp_partition/partition.c','components/esp_partition/partition_target.c','components/esp_partition/include/esp_partition.h']
report=dict(status='COMPILE_LINK_ONLY_NOT_RUN',sdk_commit=revision,sdk_status='CLEAN_TRACKED_UNTRACKED_SUBMODULES_NOT_IGNORED_FILES',
    retained_symbols=required,main_instructions=main,
    artifacts={str(p.relative_to(BUILD)):dict(bytes=p.stat().st_size,sha256=digest(p)) for p in artifacts},
    source_sha256={str(p.relative_to(ROOT)):digest(p) for p in sources},
    sdk_partition_source_sha256={p:digest(SDK/p) for p in vendor},
    development_layout=dict(flash_bytes=4*1024*1024,config_label='ag_cfg_dev',config_bytes=8192,
        history_label='ag_logs_dev',history_bytes=256*1024,ota_slot_bytes=1024*1024),
    actions=dict(build='EXECUTED',device_execution='NOT_EXECUTED',flash_read='NOT_EXECUTED',flash_write='NOT_EXECUTED',
        flash_erase='NOT_EXECUTED',format='NOT_EXECUTED',provisioning='NOT_EXECUTED',radio='NOT_EXECUTED'),
    limitations=['Empty main; real SDK flash APIs linked but never called on target',
        'Development4MiB map has256KiB log, not2MiB V1 retention or final OTA sizing',
        'No default configuration/history formatting; binding only validates trusted registered partitions',
        'Unencrypted default-chip raw data only; encrypted/read-only/overlapping/ambiguous mappings rejected',
        'Program/erase success requires synchronous SDK success and readback; physical brownout/wear/durability not qualified',
        'One serialized owner, no concurrent partition writers; IO ambiguity latches writes off until explicit maintenance/reconciliation',
        'SDK clean check excludes ignored files; hashes identify read partition implementation sources, not a full reproducible SDK archive',
        'Modern TWAI driver linked but never started; no pin/physical bus/IGN qualification or actual TX/RX',
        'Core task stack/heap and flash latency under load remain unmeasured'])
(HERE/'build-report.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: partition/TWAI adapters +runtime/discovery/config/journal cores and real SDK calls linked; main calls none')
print('Image bytes:',report['artifacts']['auto_gauge_platform_compile_study.bin']['bytes'])
