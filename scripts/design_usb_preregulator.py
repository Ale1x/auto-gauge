#!/usr/bin/env python3
"""HW-03 isolated buck study: analytical margins, schematic and real CLI exports.

Reads S-expression/library helpers from generate_review_schematic.py; never calls
its main/save routines and never writes any A0 source or export.
"""
import csv
import hashlib
import importlib.util
import itertools
import json
import math
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'hardware/usb_power/design_v01'
CLI = '/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli'


def csv_file(name, rows):
    with (OUT / name).open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)


def divider(rt, rb=10000., tolerance=.0035):
    # .1% initial + .25% TCR sensitivity for an assumed100°C excursion.
    # -3/+4% FB accuracy and400nA leakage from TPS62160-Q1.
    values = [ref * (1 + rtop / rbottom) + leak * rtop
              for ref, rtop, rbottom, leak in itertools.product(
                  (.8 * .97, .8 * 1.04),
                  (rt * (1 - tolerance), rt * (1 + tolerance)),
                  (rb * (1 - tolerance), rb * (1 + tolerance)), (-.4e-6, .4e-6))]
    return min(values), max(values)


def calculations():
    rows = []
    for target, rt in ((3.8, 37400.), (4., 40200.), (4.2, 42200.), (4.5, 46400.)):
        low, high = divider(rt)
        i33 = .5
        v33_upper_H = 3.42  # Includes room over the A0 nominal3.315V; not a frozen rail limit.
        eta1_H, eta2_H = .8, .8
        ipre = v33_upper_H * i33 / (eta2_H * low)
        iusb = v33_upper_H * i33 / (eta1_H * eta2_H * 4.5)
        dcr125_H = .0286 * (1 + .00393 * (125 - 25))
        post_mux = low - ipre * .060
        second_needed_H = v33_upper_H + i33 * (.12 + .10)
        primary_rhs_allowed = (4.5 - iusb * .131 - high) / ipre - dcr125_H
        rows.append(dict(target_H_V=target, rtop_candidate_ohm=rt,
                         actual_nominal_V=.8 * (1 + rt / 10000),
                         vpre_min_screen_V=low, vpre_max_screen_V=high,
                         i3p3_H_A=i33, v3p3_upper_H_V=v33_upper_H,
                         eta1_H=eta1_H, eta2_H=eta2_H,
                         intermediate_current_screen_A=ipre, usb_load_only_screen_A=iusb,
                         postmux_min_screen_V=post_mux,
                         tps62811_required_input_H_V=second_needed_H,
                         second_stage_margin_H_V=post_mux - second_needed_H,
                         preregulator_rhs_allowed_at_usb4p5_H_ohm=primary_rhs_allowed,
                         low_vin_rhs_max_is_missing=True,
                         status='CONDITIONAL_SCREEN_NOT_GUARANTEED'))
    csv_file('rail_tradeoff.csv', rows)
    # Effective values include assumptions about bias, tolerance and temperature.
    cin_min_H = 2 * 10e-6 * .9 * .85 * .7
    cout_min_H = 2 * 22e-6 * .8 * .85 * .7
    cout_max_H = 2 * 22e-6 * 1.2 * 1.15
    lmin_H = 3.3e-6 * .8 * .9
    selected = rows[0]
    ripple_H = selected['actual_nominal_V'] * (1 - selected['actual_nominal_V'] / 5.5) / (lmin_H * 1.5e6)
    component = dict(
        cin_parts=2, cin_nominal_each_F=10e-6, cin_min_screen_H_F=cin_min_H,
        cout_parts=2, cout_nominal_each_F=22e-6, cout_min_screen_H_F=cout_min_H,
        cout_max_screen_H_F=cout_max_H, l_nominal_H=3.3e-6,
        lmin_screen_H=lmin_H, inductor_dcr25_max_ohm=.0286,
        inductor_dcr125_screen_H_ohm=.0286 * 1.393,
        lc_corner_screen_H_Hz=1 / (2 * math.pi * math.sqrt(lmin_H * cout_min_H)),
        fsw_nominal_Hz=2.25e6, fsw_low_screen_H_Hz=1.5e6,
        ripple_screen_H_A=ripple_H,
        peak_screen_H_A=selected['intermediate_current_screen_A'] + ripple_H / 2,
        isat_25C_30percent_drop_typ_A=5.9,
        soft_start_slope_typ_V_per_s=25000,
        cap_charge_at_nominal_cout_typ_A=44e-6 * 25000,
        cap_charge_at_coutmax_typ_slope_A=cout_max_H * 25000,
        unknown=['No guaranteed DC-bias envelope for chosen capacitors',
                 'No low-VIN maximum RHS for either converter',
                 'No max soft-start slew or complete USB input-current envelope',
                 'No loss/thermal model or stability simulation'])
    (OUT / 'component_screen.json').write_text(json.dumps(component, indent=2) + '\n')
    return rows, component


def schematic():
    helper_path = ROOT / 'scripts/generate_review_schematic.py'
    spec = importlib.util.spec_from_file_location('review_helpers', helper_path)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    m.PARTS.clear(); m.PIN_MATRIX.clear()
    s = m.Sheet('USB_PREREGULATOR_STUDY', 1)
    s.items.clear()
    s.text('HW-03 / USB PREREGULATOR / H3.792V / STUDY ONLY - NOT FOR BUILD', 20, 16, 1.8)
    s.text('OPEN upstream: CC current qualification, OVP/current limit/inrush, reverse validation. NOT A COMPLETE USB SUPPLY.', 20, 27, 1.2)
    s.text('Isolated circuit: no connection to Auto Gauge A0. Apply only qualified bench input after upstream protection.', 20, 35)
    u1_pins = s.part('Regulator_Switching:TPS62160DSG', 'U1', 'TPS62160QDSGRQ1 / H', 145, 87,
           {'1':'GND_STUDY','2':'BENCH_AFTER_PROTECTION_OPEN','3':'ENABLE_QUALIFIED_OPEN',
            '4':'GND_STUDY','5':'USB_PRE_FB','6':'USB_PRE_H3V792','7':'USB_PRE_SW',
            '8':'PG_STUDY_NOT_MCU_GPIO','9':'GND_STUDY'},
           footprint='', datasheet='https://www.ti.com/lit/ds/symlink/tps62160-q1.pdf',
           mpn='TPS62160QDSGRQ1', status='H / LOW-VIN DROPOUT AND STARTUP OPEN')
    # Join PGND/AGND/PAD graphically and use one readable ground label.
    ground_ends = {(p[2], round(p[3]+7.62,6)) for p in u1_pins if p[0] in ('1','4','9')}
    s.items = [item for item in s.items if not (
        item[0]=='global_label' and item[1]=='GND_STUDY'
        and tuple(m.children(item,'at')[0][1:3]) in ground_ends)]
    endpoints = sorted(ground_ends)
    s.wire(endpoints[0],endpoints[-1])
    s.items.append(m.e('junction',m.e('at',*endpoints[1]),m.e('diameter',0),
                      m.e('color',0,0,0,0),m.e('uuid',s.key('U1 common ground'))))
    s.label('GND_STUDY',*endpoints[-1])
    s.two('Device:L','L1','3.3uH / XAL4030-332MEC H',245,85,'USB_PRE_SW','USB_PRE_H3V792',
          footprint='',mpn='XAL4030-332MEC',status='H / SOURCE STOCK0 / HOT CURVE OPEN',
          datasheet='https://www.coilcraft.com/getmedia/6adcb47d-8b55-416c-976e-1e22e0d2848c/xal4000.pdf')
    for ref,x in [('C1',40),('C2',100)]:
        m.cap(s,ref,'10uF 25V X7R H',x,154,'BENCH_AFTER_PROTECTION_OPEN',ground='GND_STUDY',
              mpn='GRM31CR71E106KA12L',footprint='Capacitor_SMD:C_1206_3216Metric',
              status='H / BIAS ENVELOPE OPEN',datasheet='https://ds.murata.com/simsurfing/mlcc.html?oripartnumbers=%5B%22GRM31CR71E106KA12L%22%5D')
    for ref,x in [('C3',220),('C4',285)]:
        m.cap(s,ref,'22uF 16V X7R H',x,154,'USB_PRE_H3V792',ground='GND_STUDY',
              mpn='CGA6P1X7R1C226M250AC',footprint='Capacitor_SMD:C_1210_3225Metric',
              status='H / BIAS ENVELOPE OPEN',datasheet='https://product.tdk.com/system/files/dam/doc/product/capacitor/ceramic/mlcc/charasheet/cga6p1x7r1c226m250ac.pdf')
    common = dict(footprint='Resistor_SMD:R_0603_1608Metric', status='H / NOT PURCHASE APPROVED',
                  datasheet='https://www.tti.com/content/dam/ttiinc/manufacturers/yageo/doc/yageo-at-anti-sulfurated-chip-resistor-datasheet-specification.pdf')
    m.resistor(s,'R1','37.4k 0.1% 25ppm H',340,85,'USB_PRE_H3V792','USB_PRE_FB',mpn='AT0603BRD0737K4L',**common)
    m.resistor(s,'R2','10k 0.1% 25ppm H',340,154,'USB_PRE_FB','GND_STUDY',mpn='AT0603BRD0710KL',**common)
    m.resistor(s,'R3','10k / EN DEFAULT OFF',40,215,'ENABLE_QUALIFIED_OPEN','GND_STUDY',mpn='AT0603BRD0710KL',**common)
    m.resistor(s,'R4','10k / PG PULLUP H',145,215,'USB_PRE_H3V792','PG_STUDY_NOT_MCU_GPIO',mpn='AT0603BRD0710KL',**common)
    s.part('Connector_Generic:Conn_01x03','J1','BENCH: VIN / GND / EN',70,85,
           {'1':'BENCH_AFTER_PROTECTION_OPEN','2':'GND_STUDY','3':'ENABLE_QUALIFIED_OPEN'},
           footprint='',in_bom=False,on_board=False,status='EXTERNAL BENCH INTERFACE ONLY')
    s.part('Connector_Generic:Conn_01x03','J2','OUT / GND / PG - NO A0 LINK',285,215,
           {'1':'USB_PRE_H3V792','2':'GND_STUDY','3':'PG_STUDY_NOT_MCU_GPIO'},
           footprint='',in_bom=False,on_board=False,status='TEST ONLY / MUX NOT CONNECTED')
    for ref,net,x in [('#FLG1','BENCH_AFTER_PROTECTION_OPEN',40),('#FLG2','GND_STUDY',100)]:
        s.part('power:PWR_FLAG',ref,'BENCH SOURCE DECLARATION',x,245,{'1':net},
               in_bom=False,on_board=False,status='EXTERNAL BENCH SOURCE ONLY')
    s.text('EN is pulled LOW. External qualified control required; PG is pulled to H3.792V, NOT a direct 3V3 MCU signal.',20,266)
    s.text('Soft start typ25mV/us can charge44uF at1.1A. OVP/current limit and CC are not implemented here.',20,274)
    s.text('FB-open clamp about7.4V exceeds TPS2116 absmax6V. This buck is NOT a single-fault OVP.',20,282)
    # Correct standalone instance paths instead of the A0 hierarchical paths.
    for item in m.children(s.items, 'symbol'):
        refs = [p[2] for p in m.children(item,'property') if p[1]=='Reference']
        if refs[0].startswith('#'):
            for prop in m.children(item,'property'):
                if prop[1] in ('Reference','Value'):
                    m.children(prop,'effects')[0].append(m.e('hide',m.A('yes')))
        for inst in m.children(item,'instances'):
            inst[:] = m.e('instances', m.e('project','usb_preregulator_study',
                        m.e('path','/'+s.id,m.e('reference',refs[0]),m.e('unit',1))))
    tree=m.e('kicad_sch',m.e('version',20250114),m.e('generator',m.A('usb_preregulator_study')),
             m.e('uuid',s.id),m.e('paper','A3'),
             m.e('title_block',m.e('title','HW03 - Isolated USB preregulator study'),
                 m.e('date','2026-10-24'),m.e('rev','HW03 H0.1'),m.e('comment',1,'NOT FOR BUILD - UPSTREAM PROTECTION OPEN')),
             m.e('lib_symbols',*s.defs.values()),*s.items,
             m.e('sheet_instances',m.e('path','/',m.e('page','1'))),m.e('embedded_fonts',m.A('no')))
    (OUT/'usb_preregulator_study.kicad_sch').write_text(m.dump(tree)+'\n')
    (OUT/'usb_preregulator_study.kicad_pro').write_text(json.dumps({'meta':{'filename':'usb_preregulator_study.kicad_pro','version':1},'sheets':[[s.id,'']]},indent=2)+'\n')
    csv_file('study_pin_matrix.csv',m.PIN_MATRIX)
    csv_file('study_candidate_bom.csv',m.PARTS)
    return len(m.PARTS), helper_path


def export_and_check():
    source=str(OUT/'usb_preregulator_study.kicad_sch')
    commands=[('netlist',['sch','export','netlist','--format','kicadxml','--output',str(OUT/'study.net.xml'),source]),
              ('erc',['sch','erc','--format','json','--output',str(OUT/'erc.json'),source]),
              ('svg',['sch','export','svg','--output',str(OUT/'svg')+'/',source])]
    for name,args in commands:
        result=subprocess.run([CLI,*args],capture_output=True,text=True)
        (OUT/(name+'.log')).write_text(result.stdout+'\n'+result.stderr)
        result.check_returncode()
    raw=ET.parse(OUT/'study.net.xml').getroot()
    nodes={(p.get('ref'),p.get('pin')):n.get('name') for n in raw.findall('./nets/net') for p in n.findall('node')}
    for row in csv.DictReader((OUT/'study_pin_matrix.csv').open()):
        if row['reference'].startswith('#'):continue
        assert nodes[row['reference'],row['pin']]==row['net'],row
    assert nodes['U1','6']==nodes['L1','2']==nodes['R1','1']==nodes['C3','1']
    assert nodes['U1','7']==nodes['L1','1']
    assert nodes['U1','5']==nodes['R1','2']==nodes['R2','1']
    assert nodes['U1','3']==nodes['R3','1']==nodes['J1','3']
    assert nodes['U1','1']==nodes['U1','4']==nodes['U1','9']=='GND_STUDY'
    erc=json.loads((OUT/'erc.json').read_text())
    assert erc.get('sheets'),'Missing full KiCad ERC report'
    findings=[v for sheet in erc['sheets'] for v in sheet['violations']]
    assert not findings, ('Unexpected standalone study ERC findings', findings)
    return dict(erc_findings=len(findings),erc_types=[v['type'] for v in findings],
                status='LOGICAL_CONNECTIVITY_ONLY_NOT_USB_OR_ELECTRICAL_QUALIFICATION',
                upstream_functions_implemented=False,linked_to_A0=False)


def main():
    OUT.mkdir(exist_ok=True)
    rows,component=calculations()
    count,helper=schematic()
    audit=export_and_check();audit['symbols']=count
    (OUT/'connectivity_audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    files=[*OUT.glob('*.kicad_sch'),*OUT.glob('*.csv'),OUT/'component_screen.json',OUT/'erc.json',OUT/'study.net.xml',helper]
    (OUT/'generated_manifest.json').write_text(json.dumps({str(p.relative_to(ROOT)):
        hashlib.sha256(p.read_bytes()).hexdigest() for p in files},indent=2)+'\n')
    print(json.dumps({'audit':audit,'rail_tradeoff':rows,'component_screen':component},indent=2))


if __name__=='__main__':
    main()
