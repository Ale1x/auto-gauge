#!/usr/bin/env python3
"""Idealized 5 V to 3.3 V comparison; not measured converter efficiency."""
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
rows=[]
for current in (0.10,0.20,0.35,0.50):
    for efficiency in (0.80,0.85,0.90):
        load=3.3*current
        rows.append(dict(load_A=current, load_W=load,
                         ldo_loss_W=(5-3.3)*current,
                         buck_efficiency_H=efficiency,
                         buck_loss_W=load*(1/efficiency-1),
                         buck_input_A_at_5V=load/efficiency/5,
                         buck_input_A_at_4_75V=load/efficiency/4.75))
out=ROOT/'evidence/models/mcu_rail_tradeoff.csv'
with out.open('w',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=list(rows[0]))
    writer.writeheader();writer.writerows(rows)
print(f'Generated {len(rows)} rail scenarios; efficiencies are hypotheses, not curves or measurements.')
