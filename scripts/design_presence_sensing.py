#!/usr/bin/env python3
"""HW-04 isolated H circuit; does not write or connect the A0 schematic."""
import csv, hashlib, importlib.util, itertools, json, subprocess
from pathlib import Path
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'hardware/presence_sensing'
CLI='/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli'
def table(name,rows):
    with (OUT/name).open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def calculate():
    # H rail 3.0..3.6V; resistor bound 1% initial + 100ppm/C*100C.
    # Datasheet does NOT supply max IB. Sensitivity columns are NOT guarantees.
    tol=.02;rows=[]
    for domain,rt,rb,vmax in [('USB',5.1e6,1e6,5.5),('OBD',13.6e6,1e6,40.)]:
        for ib in (0.,1e-9,100e-9):
            trips=[];senses=[]
            for vcc,t,b,tr,br,sign in itertools.product((3.,3.6),(rt*(1-tol),rt*(1+tol)),
                    (rb*(1-tol),rb*(1+tol)),(5.1e6*(1-tol),5.1e6*(1+tol)),
                    (1e6*(1-tol),1e6*(1+tol)),(-1,1)):
                # Two reference-input bias currents may add; signal input one.
                ref=vcc*br/(tr+br)+sign*2*ib*tr*br/(tr+br)
                input_error=.008+.025/2+ib*t*b/(t+b)
                trips.extend([(ref-input_error)*(t+b)/b,(ref+input_error)*(t+b)/b])
                senses.append(vmax*b/(t+b)+ib*t*b/(t+b))
            rows.append(dict(domain=domain,assumed_abs_IB_H_A=ib,trip_min_screen_H_V=min(trips),
                trip_max_screen_H_V=max(trips),sense_max_at_domain_vmax_H_V=max(senses),
                max_positive_domain_H_V=vmax,guaranteed=False))
    table('threshold_sensitivity.csv',rows)
    budget=dict(resistor_tolerance_H=.02,temperature_H_C=[-40,125],rail_H_V=[3.,3.6],
        obd_divider_12V_nom_A=12/14.6e6,obd_divider_12V_max_resistor_only_A=12/(14.6e6*.98),
        usb_divider_5p5_max_resistor_only_A=5.5/(6.1e6*.98),
        ref_3p6_max_resistor_only_A=3.6/(6.1e6*.98),
        both_outputs_low_3p6_pullup_max_A=2*3.6/(10000*.98),
        comparator_ICC_per_channel_max_at_VS1p8_only_A=750e-9,
        comparator_ICC_max_at_3p3_or_VCC0_A=None,input_leak_max_full_temperature_A=None,
        output_leak_max_full_temperature_A=None,
        additional_12V_standby_max_A=None,
        note='0.839uA divider bound is not a complete circuit maximum; leakage has no specified max',
        common_mode_margin_at_VCC3p0_no_IB_V=3.1-max(r['sense_max_at_domain_vmax_H_V'] for r in rows if r['domain']=='OBD' and r['assumed_abs_IB_H_A']==0),
        output_leak_allowed_H_at_VCC3p0_VIH0p75_Igpio50nA_A=.25*3/(10000*1.02)-50e-9,
        resistor_max_obd_upper_each_voltage_H_V=40*6.8e6*1.02/(13.6e6*1.02+1e6*.98),
        zero_IB_reference_tau_no_extra_cap_s=None,
        missing=['IB/ILKG and VCC0 supply current maximum over temperature',
                 'VOL max at 3.0V and GPIO leakage over full temperature',
                 'Actual AON_PROTECTED positive/negative/transient envelope',
                 'Defined validity settling time, board contamination leakage and startup qualification'])
    (OUT/'current_and_margins.json').write_text(json.dumps(budget,indent=2)+'\n')
    return rows,budget

def schematic():
    helper=ROOT/'scripts/generate_review_schematic.py'
    spec=importlib.util.spec_from_file_location('review_helpers',helper)
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    m.PARTS.clear();m.PIN_MATRIX.clear()
    cmp=m.custom('TLV7042QDGKRQ1_HW04',[(8,'VCC','power_in','L'),(4,'VEE','power_in','L'),
        (3,'INA+ USB','input','L'),(2,'INA- REF','input','L'),(5,'INB+ OBD','input','L'),
        (6,'INB- REF','input','L'),(1,'OUTA USB','open_collector','R'),(7,'OUTB OBD','open_collector','R')],
        'https://www.ti.com/lit/ds/symlink/tlv7031-q1.pdf','HW04 custom cache: Table5-2 DGK pinout; NOT A0')
    oldcmp=cmp;cmp=cmp.replace('AutoGauge:', 'PresenceStudy:')
    m.CUSTOM[cmp]=m.CUSTOM.pop(oldcmp);m.CUSTOM[cmp][1]=cmp
    s=m.Sheet('PRESENCE_SENSING_STUDY',1);s.items.clear()
    s.text('HW-04 / USB + OBD PRESENCE / H CIRCUIT / NOT FOR BUILD',20,16,1.8)
    s.text('OPEN: IB / ILKG / VCC=0 current maxima, 3V0 VOL, contamination leakage, startup validity, AON transient envelope.',20,26)
    s.text('USB RAW H0..5.5V non-PD; AON H0..40V POSITIVE ONLY. All outputs belong to switched3V3. Presence is NOT IGN.',20,34)
    common=dict(footprint='Resistor_SMD:R_0603_1608Metric',status='H / LEAKAGE MAX AND ENVIRONMENT OPEN')
    def r(ref,value,x,y,a,b,mpn):
        m.resistor(s,ref,value,x,y,a,b,mpn=mpn,
            datasheet='https://yageogroup.com/component-documentation/download/specsheet/'+mpn,**common)
    r('R1','5.1M 1% 100ppm H',65,85,'USB_RAW_H','USB_SENSE_H','RC0603FR-075M1L')
    r('R2','1M 1% H',65,144,'USB_SENSE_H','GND_STUDY','RC0603FR-071ML')
    r('R3','6.8M 1% H',145,85,'AON_PROTECTED_H','OBD_SERIES_MID','RC0603FR-076M8L')
    r('R4','6.8M 1% H',145,144,'OBD_SERIES_MID','OBD_SENSE_H','RC0603FR-076M8L')
    r('R5','1M 1% H',145,210,'OBD_SENSE_H','GND_STUDY','RC0603FR-071ML')
    r('R6','5.1M 1% H',225,85,'SWITCHED_3V3_H','REFERENCE_H','RC0603FR-075M1L')
    r('R7','1M 1% H',225,144,'REFERENCE_H','GND_STUDY','RC0603FR-071ML')
    s.part(cmp,'U1','TLV7042QDGKRQ1 / H',320,107,
        {'8':'SWITCHED_3V3_H','4':'GND_STUDY','3':'USB_SENSE_H','2':'REFERENCE_H',
         '5':'OBD_SENSE_H','6':'REFERENCE_H','1':'USB_PRESENT_H','7':'OBD_PRESENT_H'},
        mpn='TLV7042QDGKRQ1',footprint='Package_SO:VSSOP-8_3x3mm_P0.65mm',status='H / UNBOUNDED LEAKAGE / NOT QUALIFIED')
    r('R8','10k OUTPUT PULLUP H',285,210,'SWITCHED_3V3_H','USB_PRESENT_H','RC0603FR-0710KL')
    r('R9','10k OUTPUT PULLUP H',365,210,'SWITCHED_3V3_H','OBD_PRESENT_H','RC0603FR-0710KL')
    m.cap(s,'C1','100nF 16V X7R H',225,210,'SWITCHED_3V3_H',ground='GND_STUDY',
          mpn='GRM188R71C104KA01D',footprint='Capacitor_SMD:C_0603_1608Metric',status='H / LOCAL VCC DECOUPLING',
          datasheet='https://ds.murata.co.jp/simsurfing/ceramic_capacitor.html')
    s.part('Connector_Generic:Conn_01x04','J1','BENCH INPUTS ONLY',72,245,
        {'1':'USB_RAW_H','2':'AON_PROTECTED_H','3':'SWITCHED_3V3_H','4':'GND_STUDY'},in_bom=False,on_board=False)
    s.part('Connector_Generic:Conn_01x03','J2','3V3 GPIO TEST ONLY',300,245,
        {'1':'USB_PRESENT_H','2':'OBD_PRESENT_H','3':'GND_STUDY'},in_bom=False,on_board=False)
    for ref,net,x in [('#FLG1','SWITCHED_3V3_H',190),('#FLG2','GND_STUDY',230)]:
        s.part('power:PWR_FLAG',ref,'EXTERNAL BENCH SOURCE',x,245,{'1':net},in_bom=False,on_board=False)
    s.text('Pins verified to TI Table5-2: OUTA1 INA-2 INA+3 VEE4 INB+5 INB-6 OUTB7 VCC8. No A0 integration.',20,271)
    s.text('No clamp diode to3V3. Fail-safe inputs isolate VCC (TI7.4.1); numerical leakage bound still OPEN.',20,279)
    for item in m.children(s.items,'symbol'):
        ref=next(p[2] for p in m.children(item,'property') if p[1]=='Reference')
        if ref.startswith('#'):
            for p in m.children(item,'property'):
                if p[1] in ('Reference','Value'):m.children(p,'effects')[0].append(m.e('hide',m.A('yes')))
        for inst in m.children(item,'instances'):
            inst[:]=m.e('instances',m.e('project','presence_sensing_study',m.e('path','/'+s.id,m.e('reference',ref),m.e('unit',1))))
    tree=m.e('kicad_sch',m.e('version',20250114),m.e('generator',m.A('presence_study')),m.e('uuid',s.id),m.e('paper','A3'),
        m.e('title_block',m.e('title','HW04 - Isolated domain presence study'),m.e('date','2026-10-24'),m.e('rev','H0.1')),
        m.e('lib_symbols',*s.defs.values()),*s.items,m.e('sheet_instances',m.e('path','/',m.e('page','1'))),m.e('embedded_fonts',m.A('no')))
    lib=m.e('kicad_symbol_lib',m.e('version',20231120),m.e('generator',m.A('presence_study')))
    import copy
    node=copy.deepcopy(m.CUSTOM[cmp]);node[1]=cmp.split(':')[1];lib.append(node)
    (OUT/'PresenceStudy.kicad_sym').write_text(m.dump(lib)+'\n')
    (OUT/'sym-lib-table').write_text('(sym_lib_table (version 7) (lib (name "PresenceStudy") (type "KiCad") (uri "${KIPRJMOD}/PresenceStudy.kicad_sym") (options "") (descr "Isolated HW04 study cache")))\n')
    (OUT/'presence_sensing_study.kicad_sch').write_text(m.dump(tree)+'\n')
    (OUT/'presence_sensing_study.kicad_pro').write_text(json.dumps({'meta':{'filename':'presence_sensing_study.kicad_pro','version':1}},indent=2)+'\n')
    table('study_pin_matrix.csv',m.PIN_MATRIX);table('candidate_bom.csv',m.PARTS)
    return helper

def verify():
    source=str(OUT/'presence_sensing_study.kicad_sch')
    for kind,args in [('netlist',['sch','export','netlist','--format','kicadxml','--output',str(OUT/'study.net.xml')]),
                      ('erc',['sch','erc','--format','json','--output',str(OUT/'erc.json')]),
                      ('svg',['sch','export','svg','--output',str(OUT/'svg')+'/'])]:
        p=subprocess.run([CLI,*args,source],text=True,capture_output=True)
        (OUT/(kind+'.log')).write_text(p.stdout+'\n'+p.stderr);p.check_returncode()
    xml=ET.parse(OUT/'study.net.xml').getroot()
    nodes={(p.get('ref'),p.get('pin')):n.get('name') for n in xml.findall('./nets/net') for p in n.findall('node')}
    for r in csv.DictReader((OUT/'study_pin_matrix.csv').open()):
        if not r['reference'].startswith('#'):assert nodes[r['reference'],r['pin']]==r['net'],r
    assert nodes['U1','8']==nodes['R8','1']==nodes['R9','1']==nodes['C1','1']=='SWITCHED_3V3_H'
    assert nodes['U1','2']==nodes['U1','6']==nodes['R6','2']==nodes['R7','1']=='REFERENCE_H'
    assert nodes['U1','3']==nodes['R1','2']==nodes['R2','1']=='USB_SENSE_H'
    assert nodes['U1','5']==nodes['R4','2']==nodes['R5','1']=='OBD_SENSE_H'
    assert nodes['U1','1']==nodes['R8','2']==nodes['J2','1']=='USB_PRESENT_H'
    assert nodes['U1','7']==nodes['R9','2']==nodes['J2','2']=='OBD_PRESENT_H'
    erc=json.loads((OUT/'erc.json').read_text());assert erc.get('sheets')
    violations=[v for sh in erc['sheets'] for v in sh['violations']]
    assert not violations,violations
    return dict(erc_findings=len(violations),netlist_pin_assignments_verified=len(nodes),
                A0_connected=False,status='CONNECTIVITY_ONLY / ELECTRICAL AND STANDBY QUALIFICATION OPEN')
def main():
    OUT.mkdir(exist_ok=True);calculate();helper=schematic();audit=verify()
    render_runtime=Path('/Users/alessandro/Documents/hobby/pcb/tools/kicad-mcp-server/venv/bin/python')
    if render_runtime.exists():
        svg=next((OUT/'svg').glob('*.svg'))
        code='import cairosvg,sys;cairosvg.svg2png(url=sys.argv[1],write_to=sys.argv[2],background_color="white",output_width=1800)'
        subprocess.run([str(render_runtime),'-c',code,str(svg),str(OUT/'presence_sensing_render.png')],check=True)
    (OUT/'connectivity_audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    files=[Path(__file__).resolve(),helper,*OUT.glob('*.csv'),*OUT.glob('*.kicad_sch'),OUT/'study.net.xml',OUT/'erc.json',*OUT.glob('sources/*.pdf')]
    (OUT/'generated_manifest.json').write_text(json.dumps({str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},indent=2)+'\n')
    print(json.dumps(audit,indent=2))
if __name__=='__main__':main()
