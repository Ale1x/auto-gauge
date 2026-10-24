#!/usr/bin/env python3
"""Reproducible KiCad engineering review schematic; unresolved blocks stay visible."""
import copy
import csv
import json
import math
from pathlib import Path
import re
import uuid

ROOT=Path(__file__).resolve().parents[1]
HW=ROOT/'hardware'
LIBROOT=Path('/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols')
NS=uuid.UUID('f5f63132-af77-4cec-96ba-6d50321f547a')
def uid(key): return str(uuid.uuid5(NS,key))
class A(str): pass
def sexp(text):
    tokens=iter(re.findall(r'\(|\)|"(?:\\.|[^"\\])*"|[^\s()]+',text))
    def get(t):
        if t!='(': return json.loads(t) if t.startswith('"') else A(t)
        out=[]
        for t in tokens:
            if t==')': return out
            out.append(get(t))
        raise ValueError('Incomplete S-expression')
    return get(next(tokens))
def dump(x):
    if isinstance(x,list): return '('+' '.join(dump(v) for v in x)+')'
    if isinstance(x,A): return str(x)
    if isinstance(x,str): return json.dumps(x,ensure_ascii=False)
    return str(round(x,6)) if isinstance(x,float) else str(x)
def e(tag,*args): return [A(tag),*args]
def children(t,tag): return [v for v in t if isinstance(v,list) and v and v[0]==tag]
def walk(t,tag):
    for v in t:
        if isinstance(v,list):
            if v and v[0]==tag: yield v
            yield from walk(v,tag)
def effects(size=1,justify=None,hide=False):
    out=e('effects',e('font',e('size',size,size)))
    if justify: out.append(e('justify',A(justify)))
    if hide: out.append(e('hide',A('yes')))
    return out

CACHE={}; CUSTOM={}; PIN_MATRIX=[]; PARTS=[]
def standard(lib_id):
    if lib_id in CACHE: return copy.deepcopy(CACHE[lib_id])
    lib,name=lib_id.split(':',1)
    data=sexp((LIBROOT/(lib+'.kicad_sym')).read_text())
    symbols={x[1]:x for x in children(data,'symbol')}
    def resolve(n):
        node=copy.deepcopy(symbols[n]);base=children(node,'extends')
        if base:
            parent=resolve(base[0][1]);props={v[1]:v for v in children(parent,'property')}
            props.update({v[1]:v for v in children(node,'property')})
            own=[v for v in node[2:] if not(isinstance(v,list) and v[0] in ('extends','property'))]
            units=children(parent,'symbol')
            for v in units: v[1]=str(v[1]).replace(str(base[0][1])+'_',str(n)+'_',1)
            node=e('symbol',n,*[v for v in parent[2:] if not(isinstance(v,list) and v[0] in ('property','symbol'))],*props.values(),*own,*units)
        return node
    node=resolve(name);node[1]=lib_id;CACHE[lib_id]=node
    return copy.deepcopy(node)

def custom(name,pins,datasheet,description):
    """pins: (number,name,type,side); datasheet table is explicitly transcribed."""
    lib_id='AutoGauge:'+name
    body=e('symbol',name+'_0_1',e('rectangle',e('start',-12.7,22.86),e('end',12.7,-22.86),e('stroke',e('width',.254),e('type',A('default'))),e('fill',e('type',A('background')))))
    unit=e('symbol',name+'_1_1')
    sides={s:[p for p in pins if p[3]==s] for s in ('L','R')}
    for side,ps in sides.items():
        for i,(num,pname,typ,_) in enumerate(ps):
            x=-17.78 if side=='L' else 17.78;y=20.32-i*2.54
            unit.append(e('pin',A(typ),A('line'),e('at',x,y,0 if side=='L' else 180),e('length',5.08),e('name',pname,effects()),e('number',str(num),effects())))
    node=e('symbol',lib_id,e('pin_names',e('offset',.508)),e('in_bom',A('yes')),e('on_board',A('yes')),
           e('property','Reference','U',e('at',0,26,0),effects()),e('property','Value',name,e('at',0,24,0),effects()),
           e('property','Footprint','',e('at',0,0,0),effects(hide=True)),e('property','Datasheet',datasheet,e('at',0,0,0),effects(hide=True)),
           e('property','Description',description,e('at',0,0,0),effects(hide=True)),body,unit)
    CUSTOM[lib_id]=node;return lib_id

BUCK33=custom('TPS62811QWRWYRQ1',[(2,'VIN','power_in','L'),(8,'EN','input','L'),(1,'MODE_SYNC','input','L'),(7,'COMP_FSET','input','L'),(6,'SS_TR','input','L'),(4,'GND','power_in','L'),(3,'SW','output','R'),(5,'FB','input','R'),(9,'PG','open_collector','R')],
 'https://www.ti.com/lit/ds/symlink/tps62813-q1.pdf','E46 Rev.K Table5-1; candidate, package and design not frozen')
ALS=custom('OPT3001DNPRQ1',[(1,'VDD','power_in','L'),(2,'ADDR','input','L'),(3,'GND','power_in','L'),(4,'SCL','input','R'),(5,'INT','open_collector','R'),(6,'SDA','bidirectional','R')],
 'https://www.ti.com/lit/ds/symlink/opt3001-q1.pdf','E06 Rev.C Table4-1; thermal pad land mapping pending')
PROTECT=custom('LM74900QRGERQ1',[(2,'A','input','L'),(24,'C','input','L'),(22,'VS','power_in','L'),(3,'SW_SENSE','passive','L'),(4,'UVLO','input','L'),(5,'OV','input','L'),(6,'EN','input','L'),(7,'SLEEP','input','L'),(13,'GND','power_in','L'),(19,'CS_MINUS','input','L'),(20,'CS_PLUS','input','L'),(18,'ISCP','input','L'),(1,'DGATE','output','R'),(14,'HGATE','output','R'),(15,'OUT','input','R'),(16,'SLEEP_OV','input','R'),(23,'CAP','output','R'),(9,'TMR','input','R'),(10,'IMON','output','R'),(11,'ILIM','input','R'),(12,'FLT','open_collector','R'),(8,'NC','no_connect','R'),(17,'NC','no_connect','R'),(21,'NC','no_connect','R')],
 'https://www.ti.com/lit/ds/symlink/lm74900-q1.pdf','E05 Rev.C Table5-1. Exposed RTN pad must FLOAT, not GND. Parameters OPEN.')

class Sheet:
    def __init__(self,name,page):
        self.name=name;self.page=page;self.id=uid('sheet:'+name);self.defs={};self.items=[];self.counter=0;self.used=set()
        self.text('REV A0 / ENGINEERING REVIEW - NOT FOR BUILD',20,16,1.8)
    def key(self,s): self.counter+=1;return uid(self.name+':'+str(self.counter)+':'+s)
    def text(self,s,x,y,size=1.1):
        self.items.append(e('text',s,e('at',x,y,0),effects(size,'left'),e('uuid',self.key(s))))
    def wire(self,a,b):
        self.items.append(e('wire',e('pts',e('xy',*a),e('xy',*b)),e('stroke',e('width',0),e('type',A('default'))),e('uuid',self.key('wire'))))
    def label(self,net,x,y,left=False):
        # Plain labels are sheet-local. Every cross-sheet net is explicitly
        # made global; the generated netlist is checked for the connections.
        self.items.append(e('global_label',net,e('shape',A('bidirectional')),e('at',x,y,180 if left else 0),effects(.9,'right' if left else 'left'),e('uuid',self.key('label:'+net))))
    def part(self,lib_id,ref,value,x,y,nets=None,footprint=None,datasheet=None,nc=True,dnp=False,on_board=True,in_bom=True,status='CANDIDATE',mpn='TBD'):
        x=round(round(x/1.27)*1.27,6);y=round(round(y/1.27)*1.27,6)
        definition=copy.deepcopy(CUSTOM[lib_id]) if lib_id in CUSTOM else standard(lib_id)
        self.defs[lib_id]=definition
        props={p[1]:p[2] for p in children(definition,'property')}
        foot=footprint if footprint is not None else props.get('Footprint','')
        data=datasheet if datasheet is not None else props.get('Datasheet','~')
        symid=uid('component:'+ref)
        symbol=e('symbol',e('lib_id',lib_id),e('at',x,y,0),e('unit',1),e('in_bom',A('yes' if in_bom else 'no')),e('on_board',A('yes' if on_board else 'no')),e('dnp',A('yes' if dnp else 'no')),e('uuid',symid))
        for key,val,py,hidden in [('Reference',ref,y-32,False),('Value',value,y-29,False),('Footprint',foot,y,True),('Datasheet',data,y,True),('MPN',mpn,y,True),('Review_status',status,y,True)]:
            # Small passives get compact fields; ICs have room above the body.
            if ref.startswith(('R','C','L','D','Q','F','SW','TP')): py=y-7 if key=='Reference' else y-5 if key=='Value' else y
            symbol.append(e('property',key,val,e('at',x,py,0),effects(.95,hide=hidden)))
        pinlist=[]
        for p in walk(definition,'pin'):
            at=children(p,'at')[0];num=str(children(p,'number')[0][1]);pname=children(p,'name')[0][1]
            px=round(x+float(at[1]),6);py=round(y-float(at[2]),6);angle=int(at[3])
            pinlist.append((num,pname,px,py,angle,p[1]));symbol.append(e('pin',num,e('uuid',uid(ref+':pin:'+num))))
        symbol.append(e('instances',e('project','auto_gauge',e('path','/'+uid('root')+'/'+self.id,e('reference',ref),e('unit',1)))))
        self.items.append(symbol)
        nets=nets or {};attached=set()
        for num,pname,px,py,angle,typ in pinlist:
            net=nets.get(num)
            PIN_MATRIX.append(dict(sheet=self.name,reference=ref,pin=num,pin_name=pname,net=net or ('NC' if nc else 'OPEN'),status=status))
            if (px,py) in attached:continue
            attached.add((px,py))
            if net:
                dx,dy={0:(-7.62,0),180:(7.62,0),90:(0,7.62),270:(0,-7.62)}[angle]
                end=(round(px+dx,6),round(py+dy,6));self.wire((px,py),end);self.label(net,*end,left=angle==0 or (angle==90 and px<x))
            elif nc:
                self.items.append(e('no_connect',e('at',px,py),e('uuid',self.key('NC:'+ref+':'+num))))
        PARTS.append(dict(sheet=self.name,reference=ref,value=value,MPN=mpn,footprint=foot,datasheet=data,status=status,dnp=dnp,on_board=on_board,in_bom=in_bom))
        return pinlist
    def two(self,kind,ref,value,x,y,n1,n2,**kw):
        return self.part(kind,ref,value,x,y,{'1':n1,'2':n2},**kw)
    def save(self):
        tree=e('kicad_sch',e('version',20250114),e('generator',A('auto_gauge_review_generator')),e('uuid',self.id),e('paper','A3'),
               e('title_block',e('title','Auto Gauge - '+self.name),e('date','2026-10-24'),e('rev','A0 REVIEW'),e('comment',1,'NO VEHICLE CONNECTION OR FABRICATION RELEASE')),
               e('lib_symbols',*self.defs.values()),*self.items,e('embedded_fonts',A('no')))
        if self.name=='02_AUTOMOTIVE_POWER':
            children(tree,'paper')[0][1]='A2'
        (HW/(self.name+'.kicad_sch')).write_text(dump(tree)+'\n')

def cap(s,ref,value,x,y,rail,ground='GND',**kw): return s.two('Device:C',ref,value,x,y,rail,ground,**kw)
def resistor(s,ref,value,x,y,n1,n2,**kw):return s.two('Device:R',ref,value,x,y,n1,n2,**kw)
SHEETS=[]
def sheet(name):
    s=Sheet(name,len(SHEETS)+2);SHEETS.append(s);return s

def main():
    HW.mkdir(exist_ok=True)
    build_circuits()
    for s in SHEETS:s.save()
    root=e('kicad_sch',e('version',20250114),e('generator',A('auto_gauge_review_generator')),e('uuid',uid('root')),e('paper','A3'),e('lib_symbols'))
    def roottext(text,x,y,size=1.3):root.append(e('text',text,e('at',x,y,0),effects(size,'left'),e('uuid',uid('roottext:'+text))))
    roottext('AUTO GAUGE / REVISION A0 / SCHEMATIC FOR ENGINEERING REVIEW',20,20,2.4)
    roottext('Nominal 12 V OBD CAN only - one custom PCB - 1.75 inch AMOLED touch',20,30)
    roottext('User authorized schematic progression (DEC-009). K0 is NOT passed. No board layout or manufacture release.',20,40)
    roottext('OPEN: panel FPC/rails, automotive TVS/FET/SOA/limits, wake sequencing and IGN discrimination, USB protection/current.',20,50)
    roottext('TBD values and OPEN pins are deliberate blockers. ERC exceptions are reported, not hidden.',20,60)
    for i,s in enumerate(SHEETS):
        x=25+(i%2)*190;y=80+(i//2)*42
        root.append(e('sheet',e('at',x,y),e('size',160,25),e('stroke',e('width',.254),e('type',A('default'))),e('fill',e('color',0,0,0,0)),e('uuid',s.id),
             e('property','Sheetname',s.name,e('at',x,y-2,0),effects(1.3,'left')),
             e('property','Sheetfile',s.name+'.kicad_sch',e('at',x,y+27,0),effects(1,'left')),
             e('instances',e('project','auto_gauge',e('path','/'+uid('root'),e('page',str(s.page)))))))
    root.append(e('sheet_instances',e('path','/',e('page','1'))));root.append(e('embedded_fonts',A('no')))
    (HW/'auto_gauge.kicad_sch').write_text(dump(root)+'\n')
    library=e('kicad_symbol_lib',e('version',20231120),e('generator',A('auto_gauge_review_generator')))
    for key,definition in CUSTOM.items():
        node=copy.deepcopy(definition);node[1]=key.split(':')[1];library.append(node)
    (HW/'AutoGauge.kicad_sym').write_text(dump(library)+'\n')
    (HW/'sym-lib-table').write_text('(sym_lib_table (version 7) (lib (name "AutoGauge") (type "KiCad") (uri "${KIPRJMOD}/AutoGauge.kicad_sym") (options "") (descr "Verified pin tables, engineering candidates")))\n')
    for filename,rows in [('pin_matrix.csv',PIN_MATRIX),('schematic_bom_review.csv',PARTS)]:
        with (HW/filename).open('w',newline='') as f:
            writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    print(f'Generated root + {len(SHEETS)} sheets, {len(PARTS)} symbols and {len(PIN_MATRIX)} pin assignments. REVIEW ONLY.')

def build_circuits():
    # Circuit construction follows below.
    build_input();build_power();build_mcu();build_can();build_panel();build_als();build_usb()

def build_input():
    s=sheet('01_OBD_INPUT')
    s.text('Vehicle plug and 2 m harness are external wiring, not a second PCB.',20,28)
    s.text('J101 pin assignment is a proposed gauge-side map; mating orientation and actual MPN remain OPEN.',20,35)
    s.part('Connector_Generic:Conn_01x16','J100','EXTERNAL J1962 TYPE A / 12V',65,95,
           {'4':'GND','5':'OBD_SIGNAL_GND','6':'CANH_CABLE','14':'CANL_CABLE','16':'OBD_BATT_RAW'},
           footprint='',on_board=False,status='EXTERNAL HARNESS / TYPE A DRAWING OPEN')
    s.part('Connector_Generic:Conn_01x06','J101','GAUGE INPUT / MAP H',210,95,
           {'1':'BPLUS_CABLE_FUSED','2':'GND','3':'OBD_SIGNAL_GND','4':'CANH_CABLE','5':'CANL_CABLE'},
           footprint='',status='MAPPING H / HOUSINGS 0430200600 + 0430250600')
    s.two('Device:Fuse','F100','TBD FUSE / AT OBD B+',110,190,'OBD_BATT_RAW','BPLUS_CABLE_FUSED',on_board=False,status='OPEN: CURRENT / I2T / DC BREAKING')
    resistor(s,'R101','DNP / GROUND LINK TBD',230,190,'OBD_SIGNAL_GND','GND',dnp=True,status='OPEN GROUND / EMC REVIEW')
    s.part('power:PWR_FLAG','#FLG01','PWR_FLAG',55,205,{'1':'OBD_BATT_RAW'},in_bom=False,on_board=False,status='EXTERNAL SOURCE DECLARATION')
    s.part('power:PWR_FLAG','#FLG02','PWR_FLAG',55,235,{'1':'GND'},in_bom=False,on_board=False,status='EXTERNAL RETURN DECLARATION')
    s.text('No OEM pins. No K-line/J1850. CAN_H/L twisted pair; NO 120 ohm termination in this gauge.',20,245)
    s.text('F100 physically close to OBD pin 16. Fuse value, cable gauge and strain relief require fault/inrush analysis.',20,253)
    s.text('OBD pin 4 is the proposed power return. Pin 5 remains separate; R101 is DNP pending ground/EMC review.',20,261)
    s.text('Do not treat the illustrative 6-way pin numbering as a verified manufacturer mating view.',20,269)

def build_power():
    s=sheet('02_AUTOMOTIVE_POWER')
    s.text('OPEN P0: transient profiles, TVS clamp/energy, MOSFET SOA, limits and wake timing. DO NOT BUILD.',20,28,1.4)
    s.text('Common-drain topology from E05. Capacitor voltage ratings / passive values with H are engineering assumptions.',20,36)
    s.part(PROTECT,'U201','LM74900QRGERQ1',75,100,
           {'1':'IDEAL_DIODE_GATE','2':'BPLUS_CABLE_FUSED','3':'PROT_SENSE_SW','4':'PROT_UVLO','5':'PROT_OV','6':'PROT_MID',
            '7':'PROT_SLEEP_HV','9':'PROT_TMR','11':'PROT_ILIM','13':'GND','14':'LOAD_GATE','15':'AON_PROTECTED',
            '16':'PROT_MID','18':'PROT_CS_PLUS','19':'PROT_CS_MINUS','20':'PROT_CS_PLUS','22':'PROT_MID','23':'PROT_CAP','24':'PROT_MID'},
           datasheet='https://www.ti.com/lit/ds/symlink/lm74900-q1.pdf',footprint='',mpn='LM74900QRGERQ1',status='CANDIDATE / LIMITS OPEN')
    s.part('Transistor_FET:Q_NMOS_GDS','Q201','TBD 80V+ / SOA',190,90,{'1':'IDEAL_DIODE_GATE','2':'PROT_MID','3':'BPLUS_CABLE_FUSED'},footprint='',status='OPEN MOSFET SELECTION')
    s.part('Transistor_FET:Q_NMOS_GDS','Q202','TBD 80V+ / SOA',270,90,{'1':'LOAD_GATE','2':'PROT_CS_MINUS','3':'AON_PROTECTED'},footprint='',status='OPEN MOSFET SELECTION')
    resistor(s,'R201','TBD SHUNT',230,140,'PROT_MID','PROT_CS_MINUS',status='OPEN OCP / I2T')
    resistor(s,'R202','0.1 ohm H / CS FILTER',190,180,'PROT_MID','PROT_CS_PLUS')
    s.two('Device:D_TVS','D201','TBD AUTOMOTIVE TVS',50,190,'BPLUS_CABLE_FUSED','GND',status='OPEN CLAMP / ENERGY / POLARITY',footprint='')
    cap(s,'C201','100n / 100V H',75,190,'PROT_MID')
    s.two('Device:C','C202','100n / 16V H',120,190,'PROT_CAP','PROT_MID')
    resistor(s,'R203','TBD UV TOP',30,240,'PROT_SENSE_SW','PROT_UVLO',status='OPEN UV/OV LIMITS')
    resistor(s,'R204','TBD UV/OV MID',75,240,'PROT_UVLO','PROT_OV',status='OPEN UV/OV LIMITS')
    resistor(s,'R205','TBD OV BOTTOM',120,240,'PROT_OV','GND',status='OPEN UV/OV LIMITS')
    resistor(s,'R206','TBD ILIM',165,240,'PROT_ILIM','GND',status='OPEN OCP')
    cap(s,'C203','TBD TIMER',210,240,'PROT_TMR',status='OPEN FAULT TIME')
    cap(s,'C204','TBD BULK / RATING',255,240,'AON_PROTECTED',status='OPEN INRUSH / SOA')
    resistor(s,'R215','10k H / HV INPUT',30,300,'CAN_INH_HV','PROT_SLEEP_HV')
    resistor(s,'R216','100k H',75,300,'PROT_SLEEP_HV','GND')
    resistor(s,'R217','TBD WAKE DELAY',130,300,'CAN_INH_HV','BUCK5_EN_HV',status='OPEN WAKE SEQUENCE')
    cap(s,'C217','TBD / 50V',185,300,'BUCK5_EN_HV',status='OPEN WAKE SEQUENCE')
    s.text('LM74900 EP / RTN must FLOAT. Do not connect its exposed pad to GND (E05).',20,350)
    s.text('INH is HV logic, not a source for MCU. Main FET path must be ON before buck inrush; R217/C217 remain TBD.',20,358)
    s.text('Sleep bypass is for the CAN always-on load only. It is not a startup supply for MCU/display.',20,366)
    s.text('C201 / C202 are per E05. All voltage ratings, surge stress and UV/OV resistor ladder remain under review.',20,374)
    s.text('OPEN: sleep OCP latch-off may remove CAN VSUP/INH. EN tied to VS is NOT independent fault recovery.',20,382)
    s.text('Complete recovery architecture and test fault return before build; the RC wake delay alone is insufficient.',20,390)

    s.part('Regulator_Switching:LM5164DDA','U202','LM5164QDDARQ1 / 5V H',375,95,
           {'1':'GND','2':'AON_PROTECTED','3':'BUCK5_EN_HV','4':'BUCK5_RON','5':'BUCK5_FB','6':'BUCK5_PG','7':'BUCK5_BST','8':'BUCK5_SW','9':'GND'},
           datasheet='https://www.ti.com/lit/ds/symlink/lm5164-q1.pdf',mpn='LM5164QDDARQ1',status='CANDIDATE / 5V LOOP H')
    s.two('Device:L','L202','47uH H / Isat TBD',460,90,'BUCK5_SW','5V_OBD',status='H / CURRENT AND LOSS OPEN')
    cap(s,'C205','2.2u / 100V H',330,155,'AON_PROTECTED')
    s.two('Device:C','C206','2.2n / 50V X7R',375,155,'BUCK5_BST','BUCK5_SW')
    cap(s,'C207','47u / 10V H',420,155,'5V_OBD')
    resistor(s,'R207','41.2k H',465,155,'BUCK5_RON','GND')
    resistor(s,'R208','316k H',330,205,'5V_OBD','BUCK5_FB')
    resistor(s,'R209','100k H',375,205,'BUCK5_FB','GND')
    resistor(s,'R210','TBD RIPPLE RA',420,205,'BUCK5_SW','BUCK5_RIPPLE_A',status='OPEN TYPE3 LOOP')
    s.two('Device:C','C208','TBD RIPPLE CA',465,205,'BUCK5_RIPPLE_A','5V_OBD',status='OPEN TYPE3 LOOP')
    s.two('Device:C','C209','TBD RIPPLE CB',510,205,'BUCK5_RIPPLE_A','BUCK5_FB',status='OPEN TYPE3 LOOP')
    resistor(s,'R211','100k H',510,155,'5V_OBD','BUCK5_PG')
    s.text('R208/R209 => 4.992V ideal, 4.839..5.145V with ref/divider corners before ripple error.',315,230)
    s.text('R207 => about303kHz. R210/C208/C209 stay TBD; see BUCK5_FEEDBACK_REVIEW.md.',315,238)
    s.text('12V-tuned ripple network fails 12mV screening near6V. Low-VIN option adds DC error.',315,246)
    s.part(BUCK33,'U203','TPS62811QWRWYRQ1 / 3V3 H',375,285,
           {'1':'GND','2':'5V_SERVICE','3':'BUCK33_SW','4':'GND','5':'BUCK33_FB','6':'BUCK33_SS','7':'GND','8':'5V_SERVICE','9':'BUCK33_PG'},
           mpn='TPS62811QWRWYRQ1',status='CANDIDATE / E46 INITIAL VALUES')
    s.two('Device:L','L203','0.47uH H / Isat TBD',470,280,'BUCK33_SW','3V3_SYS')
    cap(s,'C210','22u / 10V H',330,345,'5V_SERVICE')
    cap(s,'C211','22u / 10V H',375,345,'3V3_SYS')
    cap(s,'C212','22u / 10V H',420,345,'3V3_SYS')
    cap(s,'C213','4.7n H',465,345,'BUCK33_SS')
    resistor(s,'R212','88.7k H',330,385,'3V3_SYS','BUCK33_FB')
    resistor(s,'R213','19.6k H',375,385,'BUCK33_FB','GND')
    s.two('Device:C','C214','10p H',420,385,'3V3_SYS','BUCK33_FB')
    resistor(s,'R214','100k H',510,345,'3V3_SYS','BUCK33_PG')
    s.text('3V3 divider => 3.315V. COMP=GND: 2.25MHz.',315,322)
    s.text('Min Ceff15uF. Validate bias/inrush/loop.',315,330)

def build_mcu():
    s=sheet('03_MCU_MEMORY_BLE')
    s.text('ESP32-S3-WROOM-1-N16R8 candidate. Internal flash/PSRAM; GPIO35/36/37 RESERVED for octal memory.',20,28)
    s.text('GPIO assignment H for review. GPIO19/20 reserved for USB; GPIO0/45/46 retain boot roles.',20,36)
    nets={'1':'GND','40':'GND','41':'GND','2':'3V3_SYS','3':'MCU_EN','4':'CAN_TXD','5':'CAN_RXD','6':'CAN_EN','7':'CAN_nSTB','8':'CAN_nFAULT',
          '9':'ALS_INT','12':'ALS_SDA','17':'ALS_SCL','18':'DISP_CS','19':'DISP_D0','20':'DISP_CLK','21':'DISP_D1','22':'DISP_D2','23':'DISP_D3',
          '10':'DISP_RESET','11':'TOUCH_INT','24':'TOUCH_RESET','25':'DISP_ENABLE','13':'USB_DN_MCU','14':'USB_DP_MCU','27':'BOOT_N',
          '38':'TOUCH_SCL_TBD_LEVEL','39':'TOUCH_SDA_TBD_LEVEL','31':'USB_PRESENT_SAFE_TBD','32':'OBD_PRESENT_SAFE_TBD','36':'UART0_RX_TEST','37':'UART0_TX_TEST'}
    s.part('RF_Module:ESP32-S3-WROOM-1','U301','ESP32-S3-WROOM-1-N16R8',145,100,nets,
           mpn='ESP32-S3-WROOM-1-N16R8',datasheet='https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf',status='CANDIDATE / PIN MAP H')
    cap(s,'C301','10u / 6.3V H',35,180,'3V3_SYS')
    cap(s,'C302','100n / 10V H',80,180,'3V3_SYS')
    resistor(s,'R301','10k',125,180,'3V3_SYS','MCU_EN')
    cap(s,'C303','1u / 10V H',170,180,'MCU_EN')
    resistor(s,'R302','10k H',215,180,'3V3_SYS','BOOT_N')
    for i,(net,ref) in enumerate([('MCU_EN','TP301'),('BOOT_N','TP302'),('UART0_TX_TEST','TP303'),('UART0_RX_TEST','TP304'),('GND','TP305')]):
        s.part('Connector:TestPoint',ref,net,40+i*65,235,{'1':net},footprint='',in_bom=False,status='TEST PAD / FOOTPRINT OPEN')
    s.text('EN RC per E02: 10k/1u initial values. Power-up/brownout timing and signed recovery require bench tests.',20,262)
    s.text('NCs on unused/reserved GPIO are deliberate. Do not assign GPIO35/36/37 to the panel on N16R8.',20,270)
    s.text('USB_PRESENT_SAFE_TBD / OBD_PRESENT_SAFE_TBD require domain-safe sensing; not raw B+ or raw VBUS.',20,278)

def build_can():
    s=sheet('04_CAN_PHY')
    s.text('Candidate TCAN1043ADRQ1; VCC from OBD-only 5V, VIO from MCU 3V3. No added termination.',20,28)
    s.part('Interface_CAN_LIN:TCAN1043xDxQ1','U401','TCAN1043ADRQ1',145,95,
           {'1':'CAN_TXD','2':'GND','3':'5V_OBD','4':'CAN_RXD','5':'3V3_SYS','6':'CAN_EN','7':'CAN_INH_HV','8':'CAN_nFAULT','9':'AON_PROTECTED',
            '10':'AON_PROTECTED','12':'CANL_CABLE','13':'CANH_CABLE','14':'CAN_nSTB'},
           mpn='TCAN1043ADRQ1',datasheet='https://www.ti.com/lit/ds/symlink/tcan1043a-q1.pdf',status='PIN TABLE CHECKED / DFM OPEN')
    cap(s,'C401','100n / 10V H',35,180,'5V_OBD')
    cap(s,'C402','100n / 10V H',80,180,'3V3_SYS')
    cap(s,'C403','100n / 50V H',125,180,'AON_PROTECTED')
    resistor(s,'R401','10k H',170,180,'3V3_SYS','CAN_TXD')
    resistor(s,'R402','100k H',215,180,'CAN_EN','GND')
    resistor(s,'R403','100k H',260,180,'CAN_nSTB','GND')
    # TVS values and RF/EMC choice are left visibly open; no terminator is added.
    s.two('Device:D_TVS','D401','TBD CAN TVS',300,180,'CANH_CABLE','GND',footprint='',status='OPEN CAN CLAMP / CAPACITANCE')
    s.two('Device:D_TVS','D402','TBD CAN TVS',350,180,'CANL_CABLE','GND',footprint='',status='OPEN CAN CLAMP / CAPACITANCE')
    s.text('WAKE tied to VSUP to disable unused local wake input (verify E03). INH goes to HV power control only.',20,232)
    s.text('EN/nSTB pull-downs favor non-normal mode on reset. Firmware must remain TX-inhibited until active state qualified.',20,240)
    s.text('CAN activity is not IGN. USB-only must not authorize requests; partial-supply leakage and modes require tests.',20,248)
    s.text('OPEN: TVS unidirectional/bidirectional network, negative CAN common-mode, capacitance, ESD and 2m signal integrity.',20,256)
    s.text('Generic two-terminal TVS symbols are selection placeholders, NOT approval of clamp polarity or a real MPN.',20,264)

def build_panel():
    s=sheet('05_AMOLED_TOUCH')
    s.text('P0 OPEN / DO NOT CONNECT FPC: ESHX0175Y003AMT003 V0 has conflicting rail/temperature data.',20,28,1.5)
    s.text('J501 is a provisional 31-way logical placeholder, excluded from BOM/board. NO physical pin mapping assigned.',20,38)
    s.part('Connector_Generic:Conn_01x31','J501','FPC31 / ALL PINS OPEN',90,115,{},nc=False,in_bom=False,on_board=False,status='BLOCKED / EXACT PANEL REVISION AND MATE OPEN',footprint='')
    nets=['DISP_CLK','DISP_CS','DISP_D0','DISP_D1','DISP_D2','DISP_D3','DISP_RESET','DISP_ENABLE','TOUCH_SCL_TBD_LEVEL','TOUCH_SDA_TBD_LEVEL','TOUCH_INT','TOUCH_RESET']
    for i,net in enumerate(nets):
        s.part('Connector:TestPoint','TP5'+str(i+1).zfill(2),net,220+(i%2)*100,70+(i//2)*30,{'1':net},in_bom=False,on_board=False,footprint='',status='LOGICAL INTERFACE ONLY')
    s.text('QSPI and touch levels/rails/sequencing: require corrected supplier specification. No devboard pinout used.',20,245)
    s.text('Touch level translation/isolation is OPEN; do not wire the 3V3 GPIO directly to a possibly 1V8 FPC.',20,255)
    s.text('Panel rails, panel power switches, FPC pin1/contact orientation and mating connector remain unassigned.',20,265)

def build_als():
    s=sheet('06_LIGHT_SENSOR')
    s.text('OPT3001DNPRQ1 / E06 Table4-1. Separate MCU I2C bus avoids assuming panel touch logic levels.',20,28)
    s.part(ALS,'U601','OPT3001DNPRQ1',145,100,{'1':'3V3_SYS','2':'GND','3':'GND','4':'ALS_SCL','5':'ALS_INT','6':'ALS_SDA'},mpn='OPT3001DNPRQ1',status='PIN TABLE E06 / LAND PATTERN OPEN')
    cap(s,'C601','100n / 10V H',45,185,'3V3_SYS')
    resistor(s,'R601','10k H',100,185,'3V3_SYS','ALS_SCL')
    resistor(s,'R602','10k H',155,185,'3V3_SYS','ALS_SDA')
    resistor(s,'R603','10k H',210,185,'3V3_SYS','ALS_INT')
    s.text('ADDR=GND => address0x44. Validate bus capacitance/rise time before fixing 10k pull-ups.',20,242)
    s.text('OPEN: optical window and calibration, thermal pad/land pattern, field of view and light leakage from AMOLED.',20,252)
    s.text('Sensor power removed with 3V3_SYS during OBD standby. No always-on ALS wake assumed.',20,262)

def build_usb():
    s=sheet('07_TEST_SERVICE')
    s.text('USB-C service only / sink / USB2. Recovery powered without OBD per DEC-007. No PD or source mode.',20,28)
    s.part('Connector:USB_C_Receptacle_USB2.0_16P','J701','USB-C / EXACT MPN OPEN',70,100,
           {'A1':'GND','A4':'USB_VBUS_RAW','A5':'USB_CC1','A6':'USB_DP_PORT','A7':'USB_DN_PORT','A9':'USB_VBUS_RAW','A12':'GND',
            'B1':'GND','B4':'USB_VBUS_RAW','B5':'USB_CC2','B6':'USB_DP_PORT','B7':'USB_DN_PORT','B9':'USB_VBUS_RAW','B12':'GND','SH':'USB_SHIELD'},
           footprint='',status='CONNECTOR MPN / VBUS PROTECTION OPEN')
    resistor(s,'R701','5.1k 1% H / Rd',30,175,'USB_CC1','GND')
    resistor(s,'R702','5.1k 1% H / Rd',85,175,'USB_CC2','GND')
    resistor(s,'R703','0R H',140,175,'USB_DP_PORT','USB_DP_MCU')
    resistor(s,'R704','0R H',195,175,'USB_DN_PORT','USB_DN_MCU')
    resistor(s,'R705','DNP / SHIELD LINK',250,175,'USB_SHIELD','GND',dnp=True,status='OPEN EMC / CHASSIS STRATEGY')
    # A protection/limit function is deliberately NOT replaced by a jumper.
    # Raw VBUS and the mux input are separate nets, leaving the blocker visible.
    s.text('OPEN SERIES FUNCTION: USB_VBUS_RAW -> current limit / OVP / controlled inrush -> USB_5V_PROTECTED_TBD',20,205,1.1)
    s.text('No wire/jumper bridges that missing protection. CC current detection and ROM-stage power budget remain OPEN.',20,213)
    s.part('Power_Management:TPS2116DRL','U701','TPS2116DRLR',275,90,
           {'1':'GND','2':'5V_SERVICE','7':'5V_SERVICE','3':'5V_OBD','4':'MUX_PR1','5':'5V_OBD','6':'USB_5V_PROTECTED_TBD'},
           mpn='TPS2116DRLR',datasheet='https://www.ti.com/lit/ds/symlink/tps2116.pdf',status='CANDIDATE / USB INPUT PROTECTION OPEN')
    resistor(s,'R706','340k H',335,165,'5V_OBD','MUX_PR1')
    resistor(s,'R707','100k H',380,165,'MUX_PR1','GND')
    cap(s,'C701','1u / 10V H',335,115,'USB_5V_PROTECTED_TBD')
    cap(s,'C702','1u / 10V H',380,115,'5V_SERVICE')
    s.two('Switch:SW_Push','SW701','RESET / HIDDEN SERVICE',40,245,'MCU_EN','GND',footprint='')
    s.two('Switch:SW_Push','SW702','BOOT / HIDDEN SERVICE',125,245,'BOOT_N','GND',footprint='')
    s.text('MUX: OBD priority H at about4.4V nominal (Vref1V; tolerance matters), MODE=VIN1 per E30.',205,236)
    s.text('Secure Boot: do not rely on ROM USB-OTG DFU. Validate USB-Serial/JTAG secure downloader + BOOT/RESET.',205,245)
    s.text('OPEN: data/CC ESD network and safe USB/OBD presence sensing. No eFuse provisioning or keys generated.',205,254)
    s.text('Rd does not grant 1.5A/3A. Verify advertised current and all pre/post-enumeration states including ROM recovery.',20,280)

if __name__=='__main__':
    main()
