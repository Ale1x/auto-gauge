#!/usr/bin/env python3
"""Record isolated SDK compile/link evidence; never starts code or accesses a device."""
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
ELF=BUILD/'auto_gauge_ble_compile_study.elf'
ARTIFACT=BUILD/'auto_gauge_ble_compile_study.bin'
HEAD='fff9895c82d744c7237be8847347bdd1b07c6643'
actual=subprocess.check_output(['git','-C',str(SDK),'rev-parse','HEAD'],text=True).strip()
assert actual==HEAD
config=(BUILD/'sdkconfig').read_text()
settings={}
for name in ['BT_ENABLED','BT_NIMBLE_ENABLED','BT_NIMBLE_MAX_CONNECTIONS','BT_NIMBLE_SM_SC_ONLY',
             'BT_NIMBLE_SM_LVL','BT_NIMBLE_SM_SC_DEBUG_KEYS','BT_NIMBLE_NVS_PERSIST',
             'BT_NIMBLE_HANDLE_REPEAT_PAIRING_DELETION','BT_NIMBLE_MAX_BONDS','BT_NIMBLE_HOST_TASK_STACK_SIZE']:
    key='CONFIG_'+name
    match=re.search(r'^'+key+r'=(.+)$',config,re.M)
    settings[key]=match.group(1) if match else 'n'
assert settings['CONFIG_BT_NIMBLE_MAX_CONNECTIONS']=='1'
assert settings['CONFIG_BT_NIMBLE_SM_SC_ONLY']=='1' and settings['CONFIG_BT_NIMBLE_SM_LVL']=='3'
assert settings['CONFIG_BT_NIMBLE_NVS_PERSIST']=='y'
assert settings['CONFIG_BT_NIMBLE_SM_SC_DEBUG_KEYS']=='n'
assert settings['CONFIG_BT_NIMBLE_HANDLE_REPEAT_PAIRING_DELETION']=='n'
nm=subprocess.check_output([str(BIN/'xtensa-esp32s3-elf-nm'),'--defined-only',str(ELF)],text=True)
required=['ag_nimble_transport_quiesce','ag_nimble_transport_start','ag_nimble_send','ag_nimble_security','ag_nimble_lookup_bond',
          'ag_nimble_advertise','ag_nimble_close','ag_nimble_bridge_init','ag_nimble_bridge_start',
          'ag_nimble_bridge_pairing_window','ag_nimble_bridge_init_bound','ag_nimble_bridge_prepare_ota','ag_nimble_bridge_prepare_rpc',
          'ag_ot_endpoint_feed','ag_ot_owner_handle','ag_device_endpoint_feed','ble_gatts_indicate_custom','nimble_port_init']
for name in required:assert re.search(r'^[0-9a-fA-F]+ [Tt] '+name+r'$',nm,re.M),name
assembly=subprocess.check_output([str(BIN/'xtensa-esp32s3-elf-objdump'),'-d','--disassemble=app_main',str(ELF)],text=True)
main=[line.strip() for line in assembly.splitlines() if re.match(r'^\s*[0-9a-fA-F]+:',line)]
assert main and not any(re.search(r'\bcall\w*\b',line) for line in main)
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
sources=[HERE/'main/nimble_transport.c',HERE/'main/endpoint_bridge.c',HERE/'main/main.c',
         HERE/'main/nimble_transport.h',HERE/'main/endpoint_bridge.h',HERE/'main/gatt_profile.h',
         ROOT/'shared/gatt.profile.json',ROOT/'firmware/src/device_endpoint.c',ROOT/'firmware/src/config_store.c',
         ROOT/'firmware/include/ag_device_endpoint.h',ROOT/'firmware/include/ota_transfer.h',
         ROOT/'firmware/include/ota_framed_endpoint.h',ROOT/'firmware/src/ota_transfer.c',
         ROOT/'firmware/src/ota_framed_endpoint.c',ROOT/'shared/c/ag_rpc.c',ROOT/'shared/c/ag_rpc.h']
report=dict(status='COMPILE_LINK_ONLY_NOT_RUN',sdk_tag='v6.1',sdk_commit=actual,
    main_instructions=main,retained_symbols=required,configuration=settings,
    artifacts={p.name:dict(bytes=p.stat().st_size,sha256=digest(p)) for p in (ELF,ARTIFACT)},
    source_sha256={str(p.relative_to(ROOT)):digest(p) for p in sources},
    runtime_actions=dict(radio='NOT_EXECUTED',NVS='NOT_INITIALIZED',flash='NOT_EXECUTED',efuse='NOT_EXECUTED'),
    limitations=['Empty app_main: public entry points are retained to verify link dependencies, never invoked',
        'Calling bridge_start/transport_start in future WOULD initialize radio; not a qualified image for flashing',
        'Hooks for qualified entropy, physical display/awake policy, restored reset journal and store are required',
        'No real ATT/SMP/iPhone/bond durability, timing, power or standby qualification',
        '8 bonds and 32768-byte host task stack are development bounds, not product N or measured sufficient stack',
        'No production restart API; faults require explicit owner handling/reboot, never automatic erase',
        'OTA requires an initialized cryptographic installer owner and fresh local maintenance policy; no wire mode switch',
        'OTA endpoint is one session per bridge lifetime; unknown cleanup or possible selection change blocks RPC rearm'])
(HERE/'build-report.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: isolated SDK compile/link, all13 public entries retained; app_main calls none')
print('Image bytes:',report['artifacts'][ARTIFACT.name]['bytes'])
