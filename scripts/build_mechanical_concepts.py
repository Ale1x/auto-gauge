#!/usr/bin/env python3
"""Reproducible conceptual drawing, not production CAD. Dimensions labeled D/H/TBD."""
from pathlib import Path
import xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[1]
mx=ET.Element('mxGraphModel',adaptiveColors='auto',page='1',pageWidth='1600',pageHeight='1080',background='#ffffff')
r=ET.SubElement(mx,'root'); ET.SubElement(r,'mxCell',id='0');ET.SubElement(r,'mxCell',id='1',parent='0')
counter=0

def box(label,x,y,w,h,style=''):
    global counter
    counter+=1
    c=ET.SubElement(r,'mxCell',id=str(counter+1),value=label,style='html=1;whiteSpace=wrap;fontFamily=Helvetica;fontSize=16;strokeColor=#375365;fillColor=#ffffff;'+style,vertex='1',parent='1')
    ET.SubElement(c,'mxGeometry',x=str(x),y=str(y),width=str(w),height=str(h),attrib={'as':'geometry'})

def text(label,x,y,w,h=40,size=16):
    box(label,x,y,w,h,f'shape=text;strokeColor=none;fillColor=none;fontSize={size};')

def circle(cx,cy,d,label='',color='#dcebf1',dash=False):
    box(label,cx-d/2,cy-d/2,d,d,'ellipse;fillColor='+color+';'+('dashed=1;' if dash else ''))

def dim(x1,y1,x2,y2,label):
    global counter
    counter+=1
    c=ET.SubElement(r,'mxCell',id=str(counter+1),value=label,style='html=1;fontFamily=Helvetica;fontSize=16;startArrow=classic;endArrow=classic;strokeColor=#375365;labelBackgroundColor=#ffffff;',edge='1',parent='1')
    g=ET.SubElement(c,'mxGeometry',relative='1',attrib={'as':'geometry'})
    ET.SubElement(g,'mxPoint',x=str(x1),y=str(y1),attrib={'as':'sourcePoint'});ET.SubElement(g,'mxPoint',x=str(x2),y=str(y2),attrib={'as':'targetPoint'})

text('<b>AUTO GAUGE / K0 — studio concettuale degli ingombri</b>',40,20,1520,55,29)
text('24-10-2026 · D = dato documentale E01/E02 · H = ipotesi · TBD = quota mancante · Nessun fit elettrico/RF o produttivo approvato',40,80,1520,40,17)
for x,title in [(270,'Fronte comune'),(800,'A / cavo posteriore'),(1330,'B / cavo laterale')]:
    text('<b>'+title+'</b>',x-215,140,430,45,23)
    circle(x,390,312,'','#f3f7fa',True)
    dim(x-156,210,x+156,210,'Ø52 mm [H]')
# Front: E01 only, do not merge alternate panel data
circle(270,390,48.96*6,'','#dcebf1')
circle(270,390,43.76*6,'AMOLED touch<br>1,75″ [D]','#163b4a')
# White lettering on dark face
list(r)[-1].set('style',list(r)[-1].get('style')+'fontColor=#ffffff;fontSize=22;')
dim(123.12,565,416.88,565,'Cover 48,96 mm [D]')
text('Margine radiale lordo 1,52 mm [H]<br>Parete 1,0 + gioco 0,3 → residuo 0,22 mm [H]',55,610,430,70)
text('ALS: apertura da definire; visibilità oltre il bordo del pannello da verificare.',55,700,430,70)
for x,option in [(800,'A'),(1330,'B')]:
    circle(x,390,288,'','#edf3ef',True)
    text('PCB Ø48 mm [H]',x-125,505,250,35)
    box('Modulo MCU<br>18 × 25,5 mm [D]',x-54,292,108,153,'fillColor=#dbe9f2;fontSize=14;')
    box('Antenna / RF<br>volume TBD',x-70,250,140,40,'dashed=1;fillColor=#fff0c9;fontSize=14;')
    box('Magnete<br>TBD',x-63,450,126,45,'dashed=1;fillColor=#fff0c9;fontSize=14;')
    text('Posizioni indicative, non coordinate di placement.',x-215,565,430,45)
    if option=='A':
        box('Presa cavo<br>posteriore / TBD',x+65,375,125,55,'dashed=1;fillColor=#fff0c9;fontSize=14;')
        text('USB-C nascosta: accesso posteriore TBD.<br>Conflitto presa / magnete / supporto da risolvere.',x-215,625,430,65)
    else:
        box('Presa + plug<br>laterali / TBD',x+120,375,125,55,'dashed=1;fillColor=#fff0c9;fontSize=14;')
        text('USB-C sotto tappo laterale / TBD.<br>Il plug può estendere l’inviluppo oltre Ø52.',x-215,625,430,65)
    text('FPC + connettore, alimentazione, CAN, ALS:<br>inviluppi da verificare; zone non assegnate.',x-215,715,430,65)
text('<b>Sezione simbolica comune — profondità totale TBD</b>',40,820,1520,45,23)
box('Pannello + cover [D E01]',65,885,350,45,'fillColor=#dcebf1;')
box('FPC / piega / aria [TBD]',435,885,350,45,'dashed=1;fillColor=#fff0c9;')
box('PCB + componenti [TBD]',805,885,350,45,'dashed=1;fillColor=#edf3ef;')
box('Case / magnete [TBD]',1175,885,350,45,'dashed=1;fillColor=#fff0c9;')
text('RF: lo spazio anulare non soddisfa da solo la raccomandazione di clearance 15 mm. Valutare la geometria 3D; nessuna antenna validata.',50,950,1500,55,18)
text('Fonti: evidence/SOURCES.md E01, E02, E23. Disegno da scripts/build_mechanical_concepts.py. Scale geometriche solo per cerchi e modulo; il resto è simbolico.',50,1020,1500,40,14)
p=root/'mechanical'/'gauge-k0-concepts.drawio';ET.ElementTree(mx).write(p,encoding='utf-8',xml_declaration=True)
assert len({c.get('id') for c in r})==len(r)
print(p)
