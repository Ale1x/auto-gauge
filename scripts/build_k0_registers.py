#!/usr/bin/env python3
"""Generate traceability/BOM from frozen source and explicit K0 assessment. No network."""
import csv
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = (ROOT / 'SPEC_V0.2.md').read_text()

# test references, planned implementation area, assessment, existing evidence
pr = {
1: ('T-004, T-008', '01_OBD_INPUT / 04_CAN_PHY / firmware', 'CANDIDATO', 'CAN_POWER_TEST_PLAN.md; E03/E07; nessun test'),
2: ('T-001, T-002, T-003', '02_AUTOMOTIVE_POWER', 'BLOCCATO', 'O-07; nessun dimensionamento protezioni'),
3: ('T-008', 'firmware catalogo PID', 'DA IMPLEMENTARE', 'DEC-003: solo Service01; discovery/decoder da verificare'),
4: ('T-008 + test allowlist negativi', 'firmware allowlist centrale', 'DA IMPLEMENTARE', 'SPEC §5.2; CAN_POWER_TEST_PLAN.md'),
5: ('T-005, T-006, T-007', 'banco e Polo effettiva', 'DA VERIFICARE', 'Nessun accesso alla vettura; nessun log'),
6: ('T-005', '04_CAN_PHY / power / stato IGN', 'BLOCCATO', 'HW-IGN / O-18'),
7: ('T-006, T-007', 'power always-on / wake', 'BLOCCATO', 'DEC-005:100µA entro300s; wake/IGN e prova aperti'),
8: ('recovery e matrice USB/OBD', '07_TEST_SERVICE', 'BLOCCATO', 'O-03; piano backfeed in CAN_POWER_TEST_PLAN.md'),
9: ('drawing + sourcing + T-010', '05_AMOLED_TOUCH', 'BLOCCATO', 'E01/E19–E22: PANEL_SOURCING_K0.md; fit e O-16 aperti'),
10: ('T-010', 'touch / firmware UI', 'DA IMPLEMENTARE', 'SPEC §6.1; nessun prototipo'),
11: ('T-010, T-011 + limiti 3x5', 'UI / validazione firmware-app', 'DA IMPLEMENTARE', 'SPEC §6.1'),
12: ('T-004, T-017', '01_OBD_INPUT / cavo / presa gauge', 'BLOCCATO', 'MPN e cablaggio da verificare; O-21'),
13: ('fit quotato + prova montaggio/RF', 'case / magnete / PCB', 'BLOCCATO', 'DEC-001; MECHANICAL_K0.md; O-22'),
14: ('T-012', '06_LIGHT_SENSOR / firmware / app', 'CANDIDATO', 'E06; curva O-12 non fissata'),
15: ('T-018', 'produzione', 'DA VERIFICARE', 'E10; RFQ non inviata'),
16: ('T-018', 'BOM / lotto 5', 'BLOCCATO', 'Nessun preventivo completo; O-19'),
17: ('T-018 + accettazione assieme', 'produzione / istruzioni FPC-cavo', 'BLOCCATO', 'O-17; SUPPLIER_RFQ_DRAFT.md'),
18: ('T-013', 'firmware autonomo / storage', 'DA IMPLEMENTARE', 'O-14 influenza storage, non autonomia UI'),
19: ('T-015', 'BLE / bootloader / flash', 'CANDIDATO', 'E02/E08; BLE transport e rollback non implementati'),
20: ('T-014', 'BLE ownership / app', 'BLOCCATO', 'O-08'),
21: ('test app/account/offline', 'app Swift / backend', 'DA SPECIFICARE', 'O-13/O-23; fuori implementazione HW K0'),
22: ('T-016', 'storage / BLE log / app', 'PARZIALMENTE DEFINITO', 'DEC-004:buffer locale limitato; LOGGING_K0.md, retention aperta'),
23: ('T-011 + editor', 'schema config / UI / app', 'BLOCCATO', 'O-10'),
24: ('T-009', 'scheduler / CAN', 'DA VERIFICARE', 'O-09; solo stime illustrative'),
25: ('timeout/stale/error injection', 'firmware catalogo / UI', 'DA IMPLEMENTARE', 'SPEC §5.3; nessun test eseguito'),
26: ('discovery simulata + primo boot', 'firmware default dashboard', 'DA IMPLEMENTARE', 'O-24; no PID preteso supportato'),
}
opened = {
1: ('decisione owner + test T-008', 'firmware diagnostico', 'SCOPE CHIUSO', 'DEC-003:solo PID live Service01; verifica stack ancora necessaria'),
2: ('decisione owner + CAD assieme', 'case / PCB / display', 'AUMENTO APPROVATO; MASSIMO FINALE APERTO', 'DEC-002: diametro esterno maggiore; Ø52 mm ipotesi, fit/tolleranze/plug aperti'),
3: ('decisione owner + test backfeed', '07_TEST_SERVICE', 'APERTA P1', 'Due opzioni in CAN_POWER_TEST_PLAN.md'),
4: ('metodo misura + T-006', '02_AUTOMOTIVE_POWER', 'TARGET APPROVATO; VERIFICA APERTA', 'DEC-005:100µA a12V entro5min OFF; STANDBY_BUDGET_K0.md'),
5: ('power budget + termica', 'power / display / MCU', 'APERTA P1', 'Esempio 2 W non budget del prodotto'),
6: ('decisione owner + T-007', 'wake / stato firmware', 'APERTA P0', 'DEC-005:standby entro5min OFF; latenza wake e comportamento ambiguo aperti'),
7: ('profili approvati + T-002/T-003', 'protezioni / case / componenti', 'APERTA P1; BLOCCA K0', 'Screening proposti; nessuna conformità'),
8: ('decisione owner + T-014', 'BLE / app / ownership', 'APERTA P1', 'Nessuna policy approvata'),
9: ('criteri owner + T-009', 'scheduler', 'APERTA P1', '1/5/15 PID da misurare'),
10: ('decisione owner + T-011', 'layout editor / schema', 'APERTA P1', 'Nessun modello approvato'),
11: ('decisione owner + test cambio auto', 'catalogo / UI', 'APERTA P2', 'PR-025 resta obbligatorio per dato indisponibile'),
12: ('decisione owner + T-012', 'ALS / brightness', 'APERTA P2', 'Curva, priorità e limiti da fissare'),
13: ('requisiti backend/app', 'cloud / account', 'APERTA P1', 'Nessuna scelta provider'),
14: ('limiti buffer + T-016', 'storage / memoria / log', 'MECCANISMO APPROVATO; DETTAGLI APERTI', 'DEC-004:buffer locale con syncBLE e carico limitato; LOGGING_K0.md'),
15: ('decisione owner + T-015', 'bootloader / distribuzione firmware', 'APERTA P2', 'E08 solo capacità piattaforma'),
16: ('datasheet corretto + campione + offerta', '05_AMOLED_TOUCH / meccanica', 'APERTA P0', 'PANEL_SOURCING_K0.md: tre candidati, nessuno congelabile'),
17: ('conferma scritta + T-018', 'fornitore assieme', 'APERTA P0', 'Bozza RFQ pronta, non inviata'),
18: ('HW-IGN: log CAN/corrente/ground truth', 'wake / CAN / power', 'APERTA P0', 'Piano prove pronto; nessun test'),
19: ('decisione owner + preventivo completo', 'economia lotto', 'APERTA P1', 'Perimetro imposte/trasporto non approvato'),
20: ('decisione destinazione prodotto', 'piano conformità', 'APERTA P1', 'Privato/commerciale non fissato'),
21: ('drawing presa/plug + T-017', '01_OBD_INPUT / case', 'APERTA P1', 'MPN non selezionato'),
22: ('drawing + prove tenuta/RF', 'magnete / antenna', 'APERTA P1', 'Nessun materiale o geometria selezionati'),
23: ('requisiti app', 'iOS / account', 'APERTA P2', 'Versione iOS e login da decidere'),
24: ('decisione owner + test primo boot', 'dashboard default', 'APERTA P2', 'Solo PID scoperti supportati'),
}

def esc(s):
    return s.replace('|', '\\|').replace('\n', ' ')

lines = ['# Tracciabilità K0 — revisione 0.4', '',
         'Data 2026-10-24. Fonte normativa SPEC_V0.2.md + DECISIONS.md (requisiti risultanti v0.2.3).',
         'Generato da scripts/build_k0_registers.py. Gli sheet sono destinazioni pianificate: non esiste ancora un progetto KiCad.',
         '«CANDIDATO» non significa selezionato o validato. Nessun test T è stato eseguito.', '',
         '| ID | Requisito / decisione | Origine | Verifica prevista | Componente / sheet correlato | Stato | Prova disponibile / mancante |',
         '|---|---|---|---|---|---|---|']
reqs = re.findall(r'^\| (PR-\d{3}) \| F \| (.*?) \|$', spec, re.M)
assert len(reqs) == 26
for key, description in reqs:
    if key == 'PR-003':
        description += ' Addendum DEC-003: solo PID live Service01.'
    if key == 'PR-022':
        description += ' Addendum DEC-004: buffer locale limitato con successiva sync BLE.'
    if key == 'PR-007':
        description += ' Addendum DEC-005: massimo100µA a12V entro5min dal quadro OFF.'
    if key == 'PR-013':
        description += ' Addendum DEC-002: piccolo aumento del diametro esterno autorizzato; massimo finale da definire.'
    test, area, state, proof = pr[int(key[3:])]
    lines.append('| ' + ' | '.join(map(esc, [key, description, 'SPEC §1.2 + DECISIONS.md' if key in ('PR-003','PR-007','PR-013','PR-022') else 'SPEC §1.2', test, area, state, proof])) + ' |')
questions = re.findall(r'^\| (O-\d{2}) \| (P[012]) \| (.*?) \|$', spec, re.M)
assert len(questions) == 24
for key, priority, description in questions:
    test, area, state, proof = opened[int(key[2:])]
    lines.append('| ' + ' | '.join(map(esc, [key, description, f'SPEC §11 ({priority})', test, area, state, proof])) + ' |')
lines.append('| O-25 | Supporto nominale 24 V escluso V1 | SPEC cronologia v0.2 e §11 | Revisione requisiti e BOM | power / scope | CHIUSA DAL PROPRIETARIO | SPEC v0.2; transitori >24 V restano da proteggere |')
lines += ['', '## Requisiti derivati senza ID originario', '',
          'Queste righe integrano, senza sostituire, le 51 voci PR/O sopra. ID D-K0 locali al registro.', '',
          '| ID locale | Origine | Verifica | Area | Stato / prova |', '|---|---|---|---|---|']
derived = [
('§2/4', 'Unico PCB custom, FPC non gateway', 'review architettura', 'tutti gli sheet'),
('§2.1', 'Nessuna terminazione extra; cavo 2 m', 'T-004 / SI / EMC', '01_OBD_INPUT / 04_CAN_PHY'),
('§3.1', 'Protezione transienti, reverse, corto, hot plug', 'T-002/T-003', '02_AUTOMOTIVE_POWER'),
('§3.1', 'Backfeed USB e rail periferiche', 'matrice quattro alimentazioni', '02 / 07'),
('§3.2', 'Nessun polling in standby; IGN non da tensione sola', 'T-005/T-006/T-007', 'CAN / wake'),
('§4.1', 'Fit quotato, RF e magneti', 'CAD e test RF', 'case / 03 / 05'),
('§5.1/5.3', 'ISO-TP, ECU multiple, timeout, bus-off e catalogo', 'T-008 + fault injection', 'firmware'),
('§6.2', 'Config versionata, validazione, rollback', 'T-011', 'firmware / app'),
('§7.1/7.4', 'Autenticazione, antireplay, OTA firmato e recovery', 'T-014/T-015', 'BLE / flash / USB'),
('§8', 'Watchdog, termica, ambiente e tenuta magnetica', 'prove da definire in O-05/O-07/O-22', 'sistema'),
('§10', 'ERC/DRC, review indipendente e package riproducibile', 'gate K2–K4', 'KiCad / CI'),
('§13', 'Nessun ordine con P0 aperti senza deroga', 'review gate e autorizzazione', 'produzione'),
]
for i, (origin, desc, test, area) in enumerate(derived, 1):
    lines.append(f'| D-K0-{i:02d} | SPEC {origin}: {desc} | {test} | {area} | DA VERIFICARE; nessun test eseguito |')
(ROOT/'traceability.md').write_text('\n'.join(lines)+'\n')

headers = ['funzione','MPN_candidato','datasheet_link','tensioni','range_termico_C','disponibilita','prezzo_unitario','quantita_per_unita','quantita_lotto_5','libreria_KiCad_o_footprint','LCSC_JLC_code','alternativa','rischio','evidenza','stato']
rows = [
['MCU BLE + memoria','ESP32-S3-WROOM-1-N16R8','https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf','3.0–3.6 V','-40…+65 standard; fino+85 con ECC PSRAM e capacità ridotta1/16','Mouser/LCSC: indicazione pubblica indicizzata; riconfermare','USD 6.72 Mouser qty1; USD 5.0752 LCSC promozione qty1','1','5','RF_Module:ESP32-S3-WROOM-1 trovato via MCP; pin/land pattern da verificare','C2913202 LCSC; JLC non confermato','WROOM con R2 o SoC separato: nuova verifica obbligatoria','ECC necessario per estensione termica; RF magnete; log/OTA non dimensionati; Wi-Fi da disabilitare','E02 E13 E14','NON CONGELATO'],
['CAN PHY con wake','TCAN1043ADRQ1','https://www.ti.com/lit/ds/symlink/tcan1043a-q1.pdf','VCC 5 V; VIO 1.7–5.5 V; VSUP da verificare su profilo scelto','-40…+125 ambiente','DigiKey 1699 indicati; non riservati','USD 1.71 qty1','1','5','Interface_CAN_LIN:TCAN1043xDxQ1 trovato via MCP; SOIC14 e pin da verificare','TBD','TCAN1043ATDRQ1; compatibilità da verificare','Wake non IGN; rail5 necessario; leakage e Vsup protetti','E03 E15','NON CONGELATO'],
['Buck automotive','LM5164QDDARQ1','https://www.ti.com/lit/ds/symlink/lm5164-q1.pdf','VIN6–100 V; uscita candidata5 V; max1 A','-40…+125 ambiente','Famiglia attiva; stock MPN TBD','TBD','1','5','Regulator_Switching:LM5164DDA trovato via MCP; PowerPAD/pin/rating variante da verificare','TBD','Altro buck automotive dopo power budget','Non regola sotto6 V; margine potenza/termica non chiuso','E04','CANDIDATO STOCK NON VERIFICATO'],
['Protezione polarità OV OC','LM74900QRGERQ1','https://www.ti.com/lit/ds/symlink/lm74900-q1.pdf','3–65 V; MOSFET esterni','-40…+125 ambiente','Famiglia attiva; stock MPN TBD','TBD','1','5','VQFN24 RGE: custom/check datasheet','TBD','Architettura a basso IQ da confrontare','Modalità normale incompatibile100µA; percorsoSLEEP/wake e TVS/SOA da validare','E05','CANDIDATO STOCK NON VERIFICATO'],
['Sensore luce','OPT3001DNPRQ1','https://www.ti.com/lit/ds/symlink/opt3001-q1.pdf','VDD1.6–3.6 V; I2C','-40…+105 grade2','Stock MPN TBD','TBD','1','5','USON6 DNP: custom/check finestra ottica','TBD','Altro ALS automotive da verificare','Apertura case e calibrazione ottica','E06','CANDIDATO STOCK NON VERIFICATO'],
['Pannello AMOLED touch','ESHX0175Y003AMT003','https://www.displayamoled.com/photo/displayamoled/document/88489/ESHX0175Y003AMT003-SPEC.pdf','NON CONGELABILI: incongruenze fra sezioni','Incongruente nel PDF; conferma richiesta','E18: MOQ1000 e4–6settimane dichiarati; cinque campioni non confermati','TBD','1','5','FPC e pinout solo dopo revisione corretta','TBD','Altro pannello1.75 equivalente; nuovo diametro da definire','Fit completo nel nuovo inviluppo non verificato; dati elettrici incompleti','E01 E18 DEC-002','CANDIDATO RIAMMESSO ALLO STUDIO; NON VALIDATO'],
]
rows += [
['Pannello AMOLED touch alternativo','ALL-UEOL018VG-RA31-A001A','https://viewedisplay.com/download/72/display-datasheet/6230/all-ueol018wv-ra31-a001a-v1-0-spec.pdf','TBD: PDF relativo a codici diversi','TBD: dati non coerenti fra pagina e PDF','Vendita diretta pubblica; stock lotto5 non confermato','USD14.99 prezzo esposto; trasporto e imposte esclusi','1 alternativa non cumulativa','5 se selezionato','Custom FPC solo dopo identità/drawing corretti','TBD','Huaxin ESHX0175Y003AMT003','VG/WV/cartiglio discordanti; no pinout congelabile','E19 E20 PANEL_SOURCING_K0.md','CANDIDATO; BLOCCATO SU IDENTITA/DATASHEET'],
['Pannello AMOLED touch alternativo','LX175A3101CTP01','','TBD da datasheet originale','-20…+70 dichiarati dal distributore; da confermare','Özdisan stock0 MOQ1 lead time4–5settimane; Eurocoin su richiesta','TBD','1 alternativa non cumulativa','5 se selezionato','TBD dopo datasheet esatto','TBD','VIEWE / Huaxin, non drop-in','Ingombro51.5mm dichiarato; no fit Ø52 con pareti ipotizzate; touch diverso','E21 E22 PANEL_SOURCING_K0.md','CANDIDATO DI RISERVA; NO STOCK/DATASHEET'],
]
rows += [
['Connettore gauge a pannello','0430200600','https://www.molex.com/en-us/products/part-detail/430200600','Da verificare con terminali/profilo transitori','-40…+105 dichiarati','DigiKey indicizzati3769; riconfermare','USD0.40 solo housing qty1','1 candidato','5','A pannello con pigtail; nessun footprint selezionato','TBD','Header PCB diretto, studio alternativo','Crimp e cablaggio interno extra; fit e strain relief non verificati','E24 E25 E27 CONNECTORS_CABLE_K0.md','CANDIDATO NON CONGELATO'],
['Housing femmina cavo gauge','0430250600','https://www.molex.com/en-us/products/part-detail/0430250600','Da verificare con terminali/profilo transitori','Da confermare variante','DigiKey indicizzati85648; riconfermare','USD0.51 solo housing qty1','1 candidato','5','Cablaggio; nessun footprint','TBD','Presa esterna diversa da verificare','Terminali venduti separatamente; protezione contatti alimentati e mating da verificare','E24 E28 CONNECTORS_CABLE_K0.md','CANDIDATO NON CONGELATO'],
['Connettore gauge PCB alternativo4p','0430450400','https://www.molex.com/en-us/products/part-detail/0430450400?display=pdf','Rating pagina600V; verifica transitori e sistema necessaria','-40…+105 dichiarati','Stock indicizzato; MOQ/prezzo UE TBD','TBD','1 alternativa non cumulativa','5 se selezionato','THT right angle; footprint/pin e meccanica da verificare','TBD','Micro-Fit6p a pannello','Solo4vie richiede decisione masse;30cicli dichiarati; no fit dimostrato','E26 CONNECTORS_CABLE_K0.md','ALTERNATIVA NON CONGELATA'],
]
pending = [
('TVS ingresso automotive','Energia e clamp dopo O-07','02_AUTOMOTIVE_POWER'),
('MOSFET protezione','VDS SOA leakage e inrush dopo O-07','02_AUTOMOTIVE_POWER'),
('Fusibile cablaggio','Proteggere cavo vicino al prelievo senza secondo PCB','01_OBD_INPUT'),
('Regolatore3.3V e rail pannello','Power budget e sequenza da chiudere','02_AUTOMOTIVE_POWER / 05_AMOLED_TOUCH'),
('TVS CAN e filtro EMI','Capacità clamp e SI da validare','04_CAN_PHY'),
('Plug J1962 Type A + cavo2m','Famiglia E16 non prova MPN/cablaggio','01_OBD_INPUT'),
('Terminali crimp connettore gauge','MPN dopo sezione isolamento placcatura e attrezzaggio; housing non completi','01_OBD_INPUT'),
('Connettore FPC','Vietato scegliere da foto o devboard','05_AMOLED_TOUCH'),
('USB-C + ESD + power path','Decisione O-03 e fit; no backfeed','07_TEST_SERVICE'),
('Passivi induttori condensatori','Rating derating EMC e temperature da dimensionare','02_AUTOMOTIVE_POWER'),
('Case magnete supporto','Fit nuovo diametro (Ø52 ipotesi) e RF; costo assieme','MECHANICAL_K0.md'),
]
for function,risk,area in pending:
    rows.append([function,'TBD','','TBD','TBD','NON VERIFICATA','TBD','TBD','TBD',f'Da definire: {area}','TBD','TBD',risk,'SPEC v0.2 / K0-001','BLOCCATO / NESSUN MPN SELEZIONATO'])
assert all(len(r) == len(headers) for r in rows)
with (ROOT/'BOM_CANDIDATES.csv').open('w', newline='') as f:
    writer = csv.writer(f, lineterminator='\n'); writer.writerow(headers); writer.writerows(rows)
manifest = {}
for p in sorted((ROOT/'evidence').glob('*')):
    if p.suffix == '.pdf':
        manifest[p.name] = {'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size}
(ROOT/'evidence'/'manifest.json').write_text(json.dumps({'retrieved_on':'2026-10-24','files':manifest},indent=2)+'\n')
print(f'Generated 26 PR + 25 O + {len(derived)} derived requirements; {len(rows)} BOM rows; {len(manifest)} source PDFs.')
