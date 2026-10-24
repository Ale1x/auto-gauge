#!/usr/bin/env python3
"""Build bounded demo pages from shared regression fixtures, without changing shared/."""
import copy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
scenarios = {s['name']: s for s in json.loads((ROOT/'shared/fixtures/log.scenarios.json').read_text())}
loss_page = next(o['page'] for o in scenarios['declared_loss_and_late_record']['operations'] if o['accepted'])
second_session = scenarios['session_isolation']['operations'][1]['page']
timeout_page = copy.deepcopy(next(o['page'] for o in scenarios['invalid_status_raw']['operations'] if o['accepted']))
timeout_page['session_id'] = '03'*16
output = ROOT/'mobile/Sources/GaugeCore/Resources/log-demo-pages.json'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(dict(device_id='SIMULATO-001',pages=[loss_page,second_session,timeout_page]),indent=2)+'\n')
print(output)

rpc = json.loads((ROOT/'shared/fixtures/rpc.exchanges.json').read_text())
rpc_output = output.parent/'rpc-demo-fixture.json'
rpc_output.write_text(json.dumps({key:rpc[key] for key in ('initial_configuration','history_source')},indent=2)+'\n')
print(rpc_output)

profile_output = output.parent/"gatt.profile.json"
profile_output.write_bytes((ROOT/"shared/gatt.profile.json").read_bytes())
print(profile_output)
