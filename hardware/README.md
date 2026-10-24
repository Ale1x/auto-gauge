# Auto Gauge — schema KiCad A0

**Revisione ingegneristica incompleta, non destinata alla costruzione.** Creazione autorizzata da DEC-009. K0 non superato: dati essenziali e prove restano aperti. Verificato con KiCad CLI10.0.7.

Aprire `auto_gauge.kicad_pro` oppure `auto_gauge.kicad_sch`. Il progetto contiene un foglio principale e sette fogli gerarchici collegati con etichette globali. `exports/auto_gauge_A0.pdf` è il PDF di8pagine con link gerarchici. Nessun filePCB, layout o routing.

| Foglio | Contenuto A0 | Aperture sostanziali |
|---|---|---|
| 01_OBD_INPUT | J1962 esterno, fusibile al prelievo, mappa proposta del6vie gauge, riferimenti4/5 distinti | Codici/drawing, dimensionamento fusibile/cavo, ground strategy |
| 02_AUTOMOTIVE_POWER | Topologia LM74900 e MOSFET common-drain; buckLM5164 5V e TPS62811 3,3V | TVS/MOSFET/SOA, limitiUV/OV/OCP, ripple network5V e sequenza wake, valoriTBD |
| 03_MCU_MEMORY_BLE | WROOM-N16R8, alimentazione/EN, boot, test, assegnazione GPIO candidata | SensingUSB/OBD sicuro, prove power-up/RF/firmware; pin map non congelata |
| 04_CAN_PHY | TCAN1043A, VCC daOBD eVIO3,3V, INH ad alta tensione, bias e decoupling | ProtezioniCAN e SI2m; rilevamentoIGN non dimostrato |
| 05_AMOLED_TOUCH | Interfacce logiche e segnaposto31vie | **Nessun pinFPC assegnato**, rail/livelli/sequenze e connettore non verificati |
| 06_LIGHT_SENSOR | OPT3001, busI2C separato, pull-up e bypass | Finestra ottica, pad/footprint e misura rise-time |
| 07_TEST_SERVICE | USB-C sink, Rd, dati versoMCU, muxTPS2116, BOOT/RESET | **VBUS raw non collegato al mux**: manca la funzioneOVP/limite corrente/inrush; CC/ESD e recovery sicura da verificare |

## Collegamenti e distinte

`pin_matrix.csv` elenca336 pin/assegnazioni, incluse NC eOPEN. `schematic_bom_review.csv` elenca97 simboli con stato,MPN e datasheet dove noti: contiene segnaposto, passivi non selezionati, parti esterne e simboli non destinati alla BOM. Non usarla come distinta per ordini o PCBA. `BOM_CANDIDATES.csv` nella radice resta il registro di ricerca separato.

I valori marcatiH sono ipotesi: per esempio i partitori iniziali dei buck non costituiscono dimensionamento completo dei regolatori. I valoriTBD bloccano il funzionamento sicuro dell’assieme. Il simboloTVS generico non seleziona polarità, capacità o clamp di un componente reale. Non ponticellare la funzioneUSB mancante e non alimentare il circuito come se fosse completo.

Il simboloFPC J501 e i testpoint logici del foglio5 sono esclusi daBOM/board e servono a rendere evidente la mancanza di pinout. Le parti di cablaggio esterne non sono una seconda scheda. Mappa del6vie e assegnazioneGPIO sono proposte della revisioneA0, non dichiarazioni del fornitore.

**Rischio di fault recovery ancora aperto:** se una sovracorrente inSLEEP porta il controller inlatch-off e fa collassareAON, il transceiver può perdereVSUP eINH. EN collegato aVS non fornisce un reset indipendente. Il solo ritardoRC di wake non risolve questa condizione: occorre completare l’architettura di recupero prima della costruzione, mantenendo il budget100µA. Non è dimostrata ripartenza autonoma da quel guasto.

`AutoGauge.kicad_sym` contiene simboli trascritti dalle tabelle originali per LM74900-Q1, TPS62811-Q1 eOPT3001-Q1; i datasheet sono incorporati nelle proprietà. Le restanti definizioni sono incluse nelle cache dei fogli; non sono state modificate librerie di sistema. PerLM74900 il padRTN deve rimanere flottante: il footprint non è assegnato. Non trasferire la regolaEP del buckLM5164 a questo componente.

## Verifiche eseguite e limiti

KiCad apre tutti gli8fogli ed esporta PDF,SVG e netlistXML. Il controllo di connettività confronta la netlist effettiva con273 assegnazioni e verifica USB duplicataA/B, Rd distinti, CAN_H/L, railCAN daOBD, GPIO35/36/37 riservati, assenza di120Ω e separazione fraUSB_VBUS_RAW e l’ingressoUSB del mux. Risultato in `exports/connectivity_audit.json`.

**ERC:36errori e2warning aperti, non esclusi né approvati.** Dettagli in `ERC_REVIEW.md` e `exports/erc.json`. Il controllo script classifica gli errori attesi ma non converte il risultato inPASS elettrico. Nessun test hardware, simulazione di circuito, conformità automotive o verifica meccanica eseguiti. È stata svolta una revisione documentale limitata da un subagente separato: `reviews/A0_INDEPENDENT_REVIEW.md`, con 53 collegamenti critici confrontati e riscontri HR-01…HR-08. Non equivale a revisione completa o approvazione del circuito.

## Riproduzione

Dalla radice del repository:

```sh
python3 scripts/generate_review_schematic.py
/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli sch export netlist --format kicadxml --output hardware/exports/auto_gauge.net.xml hardware/auto_gauge.kicad_sch
/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli sch erc --format json --output hardware/exports/erc.json hardware/auto_gauge.kicad_sch
/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli sch export pdf --output hardware/exports/auto_gauge_A0.pdf hardware/auto_gauge.kicad_sch
python3 scripts/check_review_schematic.py
```

Il generatore usa le librerie KiCad installate nel percorso indicato nello script. GliUUID e le sorgenti generate sono riproducibili a parità di script/librerie. Le date degli export e i metadatiPDF/netlist possono variare. Le modifiche manuali ai fogli generati vengono sostituite dalla rigenerazione: riportarle prima nello script. Nessuna automazione genera PCB o ordini.

Prima di una revisione costruibile servono datasheet pannello corretto, chiusura potenza/protezioni/USB/wake, MPN/footprint e passivi reali, verifica completa della netlist e review indipendente. L’ERC pulito da solo non basta.

## Approfondimento del buck 5 V

`../BUCK5_FEEDBACK_REVIEW.md` e `../evidence/models/buck5_ripple_screen.*` confrontano due reti di feedback su 16 punti e 64 combinazioni di tolleranze per punto. La rete studiata per 12 V perde margine sul ripple verso 6 V; quella a ripple maggiore richiede di valutare l’errore DC. I passivi di feedback restano TBD. Il foglio 02 riporta ora questi limiti. L’archivio di revisione include analisi e relativo script; non contiene nuovi componenti approvati.

## Studio USB e wake HW-02

`usb_power/HW02_REVIEW.md` contiene il confronto di due protezioni USB, l'alternativa con preregolatore e tre possibili architetture di recupero dal fault SLEEP. I calcoli sono riproducibili con `python3 scripts/analyze_usb_power.py` dalla radice; CSV, PDF originali e relativi hash sono nella directory di studio. Lo studio respinge l'OVP diretta come soluzione completa e non modifica lo schema A0. HR-01/HR-02 restano aperti. La directory di studio è versionata separatamente; l'archivio A0 continua a contenere lo schema e la sua review, non un nuovo circuito USB.
