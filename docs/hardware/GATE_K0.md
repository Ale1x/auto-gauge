# Gate K0 — revisione 0.14, 2026-10-24

**Esito: NON SUPERATO / BLOCCATO.** Questo è il primo rapporto di fattibilità, non un design freeze. DEC-009 autorizza ad avanzare allo schema di revisione con blocker aperti: hardware/auto_gauge.kicad_sch, revisioneA0. K0 non viene dichiarato superato. Nessuno schematico definitivo, PCB, routing o ordine avviato.

Base requisiti: docs/product/SPEC_V0.2.md + docs/project/DECISIONS.md fino a DEC-014 (v0.2.8). Il proprietario autorizza un piccolo aumento del diametro esterno oltre 44,45 mm. Ø52 mm è ipotesi di studio; massimo finale, tolleranze e inviluppo del plug restano aperti. PR-009 è invariato. DEC-011 consente N client Bluetooth in momenti diversi con una sola connessione attiva; nessun reset per cambiare telefono. DEC-012 sceglie iCloud/CloudKit senza account email separato; DEC-013 approva team e identificatori Apple per lo sviluppo, senza pubblicazione App Store. DEC-014 approva consenso locale/codice per i telefoni nuovi e riconnessione di quelli riconosciuti.

## Esito degli otto task correnti

| Task | Stato | Risultato / documento |
|---|---|---|
| 1 Tracciabilità | COMPLETATO come registro iniziale | docs/project/traceability.md: 26 PR, 25 O, 12 derivati locali. Nessuna validazione hardware dichiarata |
| 2 Pannello nudo | BLOCCATO | Tre candidati in docs/suppliers/PANEL_SOURCING_K0.md; VIEWE ha vendita pubblica e PDF, ma codici discordanti. Nessuna identità/fornitura congelata |
| 3 BOM candidata | PARZIALE | hardware/BOM_CANDIDATES.csv:25 candidati/righe, inclusi i due terminali crimp prima TBD; stock/prezzi solo dove supportati da fonte, nessun MPN congelato |
| 4 CAN/cavo | PARZIALE | Strategia e budget illustrativi in docs/hardware/CAN_POWER_TEST_PLAN.md; conformità dello stack, connettori e ramo2m non verificati |
| 5 Automotive12V/wake | BLOCCATO | Profili e target proposti, nessun dimensionamento completo né misura. target100µA/300s approvato; prova e O-06/O-07/O-18 ancora aperte |
| 6 Meccanica | BLOCCATO | docs/hardware/MECHANICAL_K0.md: tavola concettuale A/B in hardware/mechanical/gauge-k0-concepts.drawio.png; quote note e ipotesi distinte, fit completo ancora aperto |
| 7 Fornitori/costo | PARZIALE | Fonti pubbliche JLCPCB/PCBWay/Seeed verificate; docs/suppliers/SUPPLIER_CONTACT_LOG.md:tutti e quattro contattati; Huaxin ha risposto sul pannello, assemblatori in attesa. Nessuna conferma manual-assembly/sourcing specifica né preventivo |
| 8 Gate report | COMPLETATO come report preliminare | Questo file e docs/hardware/issues/K0-001_BLOCKERS.md; exit gate non raggiunto |

## Fattibilità e numeri

Con DEC-002 il precedente limite numerico è superato. Il candidato E01 può rientrare nel nuovo studio: il calcolo radiale con parete ipotetica 1 mm e gioco 0,3 mm suggerisce almeno Ø51,56 mm, quindi Ø52 mm come prima ipotesi. Non è un fit validato; vedere docs/hardware/MECHANICAL_K0.md.

Plausibile ma non dimostrato: MCU/BLE + CAN su unico PCB, UI e OTA. ESP32-S3-WROOM-1-N16R8: +65°C tabellari, fino+85°C con ECC PSRAM e capacità ridotta (E02); configurazione e limiti del prodotto da verificare. Rettifica del precedente richiamo incompleto: docs/hardware/MEMORY_RF_K0.md. Non assumere qualifica automotive del modulo o dell'intero assieme.

Non verificabile con le evidenze disponibili: riconoscimento affidabile del quadro sulla Polo, wake automatico, settimane in standby, immunity automotive, prestazioni5Hz, RF col magnete, costo40–70EUR e consegna assemblata completa.

Misurato sul prodotto: **nulla**. Dati disponibili: nominali da datasheet, pagine pubbliche di sourcing e calcoli riproducibili. Strumentazione, campioni e vettura non disponibili in questa sessione. Gli esempi di potenza, corrente e traffico non sono misure.

## Decisioni e azioni P0 ancora necessarie

| ID | Chiusura richiesta |
|---|---|
| O-01 | CHIUSA nello scope da DEC-003: solo PID live Service01; restano implementazione e test |
| O-04 | Target approvato DEC-005:100µA a12V entro5min OFF; metodo completo e prova da chiudere |
| O-06 | Standby entro5min OFF approvato; latenza wake e comportamento CAN/IGN ambiguo ancora aperti |
| O-14 | DEC-004 approva buffer locale limitato con syncBLE; DEC-006 approva2MiB condizionatiOTA, fino1Hz, overwrite con segnalazione; sizing/prova ancora aperti |
| O-16 | Sourcing/ingegneria: pannello realmente conforme, datasheet corretto, campione e fornitura lotto5 |
| O-17 | Fornitore: conferma scritta e prezzo per assemblaggio completo; invio autorizzato; stato in docs/suppliers/SUPPLIER_CONTACT_LOG.md |
| O-18 | Prova HW-IGN: dimostrare comportamento non proprietario e concordare eventuale compromesso |

O-02: diametro esterno confermato come grandezza; aumento autorizzato, massimo numerico finale, tolleranze e inviluppo connettore da fissare. O-07, pur classificata P1 in SPEC, blocca tecnicamente K0 per la stop condition sulle protezioni. O-05/O-19/O-21/O-22 e requisiti ambientali vanno chiusi per un freeze credibile. Il registro completo è docs/project/traceability.md.

## Assunzioni vietate e limiti rispettati

Non usare devboard come pinout pannello; non congelare un nuovo diametro senza fit e decisione finale; non ridurre il display; non equiparare CAN/tensione a IGN; non fare polling per svegliare il veicolo; non aggiungere120Ω; non garantire15PID a5Hz; non estendere il nominale a24V; non rimuovere protezioni contro transitori>24V; non dichiarare compatibilità Polo o servizi di assemblaggio dalla sola pagina marketing; non considerare ERC/DRC prova sufficiente; non ordinare.

## Piano per chiudere K0 e accedere a K1

1. Verificare E01 nel nuovo inviluppo consentito da DEC-002 o trovare alternativa equivalente: correggere le incongruenze del datasheet e acquisire disponibilità/prezzo/MOQ e campione. Il piccolo aumento di diametro non risolve questi blocchi.
2. Completare due studi quotati reali con pannello/PCB/FPC, presa e plug gauge, USB, magnete, ALS e antenna. Verificare fit e RF prima di congelare footprint.
3. Applicare DEC-003–005 e completare O-04/metodo, O-06, O-07, O-14/limiti buffer e condizioni d’uso USB; fissare severità e criteri, poi chiudere power/thermal budget.
4. Eseguire piano HW-IGN prima su simulatore e poi sulla Polo dopo controlli di banco; raccogliere corrente e log con ground truth. Nessun test su vettura già eseguito.
5. Con autorizzazione al contatto, richiedere fattibilità/quote complete ai fornitori usando la bozza; ottenere conferma sourcing e montaggio manuale. Nessun ordine.
6. Aggiornare tracciabilità e BOM con evidenze e alternative verificate; chiudere blocker. Solo dopo approvazione K0 iniziare K1: rail, pin matrix, power/RF/thermal budget e BOM fissata. K2 creerà il progetto tramite KiCad MCP con gerarchia SPEC §10; K3 layout, K4 package e preventivo definitivo, K5 solo dopo approvazione produzione.

## Riproducibilità

Eseguire `python3 scripts/build_k0_registers.py` per rigenerare registro, CSV e manifest PDF; `python3 scripts/build_feasibility_models.py` e `python3 scripts/build_power_envelope.py` per gli studi parametrici; `python3 scripts/check_k0.py` per controlli documentali e calcoli. Il controllo non è ERC/DRC né validazione elettrica. Le fonti web sono datate e richiedono nuova verifica al freeze. Nessuna pubblicazione remota; sorgenti versionate inGit locale. La firma1Password è riuscita nella sessione di creazione dello schemaA0, superando il precedente blocco del commit.

## Avanzamento successivo

Ricerca pannelli estesa a tre candidati e tre canali di fornitura, con richiesta tecnica mirata pronta in docs/suppliers/PANEL_SOURCING_K0.md. BOM aggiornata con alternative non cumulative. docs/hardware/MEMORY_RF_K0.md quantifica framebuffer e sensibilità dei log e rettifica il rating condizionato ECC. Inventario librerie KiCad eseguito in sola lettura. La tavola meccanica è concettuale: la presenza di quote non chiude fit, RF o sourcing. O-16/O-17/HW-IGN e target standby rimangono aperti; nessun ulteriore gate approvato.

## Decisioni e analisi successive

DEC-003–005 hanno fissato Service01, buffering locale limitato con syncBLE, e target100µA a12V entro5min OFF. docs/hardware/LOGGING_K0.md propone partizione/coda fisse e decimazione senza rallentare CAN/UI; valori H di implementazione e retention da verificare; policy overflow poi approvata DEC-006. docs/hardware/STANDBY_BUDGET_K0.md respinge la protezione sempre in modalità normale e analizza i consumi del percorso sleep. docs/hardware/CONNECTORS_CABLE_K0.md introduce una coppia6vie a pannello come candidata; terminali, plugOBD, fit e cablaggio restano aperti. docs/hardware/HW_IGN_TEST_PROTOCOL.md e template CSV pronti; nessun test eseguito.

## Stato corrente dopo DEC-006/007

Policy log e alimentazione USB recovery approvate e propagate a BOM/registro. docs/hardware/USB_RECOVERY_K0.md definisce i percorsi candidati e la matrice prove. PDF originaleLX acquisito ma rail incoerenti; nessun pannello congelato. Huaxin ha risposto (E32): campioni a stock dichiarati, USD10/unità, chiede indirizzo per trasporto. PDF allegato ancoraV0: unica differenza testuale rilevata da E01 è LCD→OLED in copertina; incongruenze irrisolte. Ecer chiede informazioni (E33), senza offerta. Follow-up Huaxin inviato con destinazione autorizzata e chiarimenti tecnici; JLCPCB/PCBWay inviati via Gmail, conferma SENT. Non serve nuova autorizzazione. K0 resta non superato; nessun test hardware o ordine.

## Approfondimento capacità e invii assemblaggio

docs/hardware/CAPACITY_K0.md e tre CSV riproducibili quantificano12 scenari flash,6 log e15 latenza CAN. Con16MiB, due slot da5MiB, log2MiB, asset4MiB e riserva256KiB non entrano (deficit0,25MiB); con slot4MiB entrano aritmeticamente lasciando1,75MiB. Nessuna immagine firmware dimensionata o tabella partizioni congelata. Nel modello a blocchi, record32byte e cinquePID/1Hz danno3,57ore;16byte non è un formato universale garantito. Con cinquePID e transazioni seriali da100ms il massimo aritmetico è2Hz/PID, prima di altri ritardi. Non si garantisce5Hz dal solo carico CAN in bit/s.

Tutti e quattro i contatti autorizzati risultano eseguiti. Nessuna risposta degli assemblatori ancora acquisita e nessun budget40–70EUR verificato. I blocker che richiedono datasheet corretto, offerta o misure restano aperti; nessun passaggio a K1/K2.

## Potenza, cavo e recovery al crank — DEC-008

docs/hardware/POWER_ENVELOPE_K0.md quantifica288 scenari a carico ipotetico: modello resistivo del cavo2m, perdite conversione e margini del buck. A6V alla presa, il cavo porta l’ingresso del gauge sotto il minimo6V del buck candidato. DEC-008 ammette riavvio durante crank con ripartenza automatica; livelli/durate e tempo massimo restano aperti. La classe funzionale non è più una domanda aperta, mentre integrità e recupero richiedono prove.

Individuato un vincolo della catena wake: il bypass SLEEP del LM74900-Q1 non è un percorso valido da assumere per l’avvio dell’intero carico; uscire da SLEEP prima del carico principale, verificando INH, default, inrush e latch-off. Non è ancora uno schema dimensionato.

Preparato scripts/analyze_hw_ign.py con4 test software su fixture sintetiche; template di misura ancora vuoto e risultato NOT_DETERMINED. Lo strumento non emette PASS hardware. Nessuna misura o acquisto eseguito.

## Connettori: MPN contatti e rischio durabilità

Documentati0430310007/0430300007 e attrezzaggio Molex; la scelta finale richiede filo/isolamento/crimp verificati. Il limite30cicli riguarda anche i terminali della coppia a pannello. Confrontata HiroseHR30 da1000cicli, ma il plug risulta senza stock e la coppia costa indicativamenteUSD23,42 prima del cablaggio: nessuna sostituzione definitiva. docs/hardware/CONNECTORS_CABLE_K0.md distingue costi e condizioni. Il download drawingMolex resta fallito, nessun cutout ricavato da estrazione ambigua. O-21 resta aperta. Verifica Gmail24/10/2026: nessuna nuova risposta rispetto alla prima Huaxin già registrata; nessun reinvio duplicato.

Audit librerie:14nomi/numero pin TCAN1043xDxQ1 corrispondono alla tabella del candidatoTCAN1043ADRQ1. Datasheet del simbolo generico non-A e land pattern diverso dall’esempioTI richiedono verifica prima del freeze; risultati e hash in evidence/kicad-tcan-library-audit.json. Nessun simbolo/footprint modificato e nessun progetto creato.

RicercaOBD/cavo: individuato SKU KDS33114 TypeA, ancora senza documentazione tecnica completa; separato dalla voce cavo2m in BOM. Cavi commerciali con quattro fili o solo rating di segnale non chiudono potenza/masse/twist. RichiestaKDS inviata dopo consenso esplicito aggiuntivo; GmailSENT confermato, dettagli in docs/suppliers/SUPPLIER_CONTACT_LOG.md. Nessun blocker chiuso o ordine.

## Rail3,3V e recoveryUSB

docs/hardware/MCU_RAIL_K0.md documenta il requisito documentale della sorgenteMCU≥0,5A, confronta LDO/buck e proponeTPS62811QWRWYRQ1 come candidato non congelato; la BOM separa ora questo rail dai rail pannello ancoraTBD. Dodici scenari calcolati con efficienze ipotetiche, nessuna curva misurata. La scelta di un LDO500mA come unico railMCU+periferiche non ha margine documentato.

docs/hardware/USB_RECOVERY_K0.md distingueUSB-Serial/JTAG eROMDFU: non usare quest’ultima come unica recovery conSecureBoot/flash encryption. PoliticaeFuse, accesso manuale al downloader, corrente consentita dalla sorgenteUSB e recupero immagine firmata restano da verificare. Il rischio viene reso esplicito prima dello schema; nessuna sicurezza disabilitata o eFuse programmata.

## Schema KiCad A0 — autorizzazione DEC-009

Creati viaMCP progetto e fogli, poi generati in modo riproducibile con scripts/generate_review_schematic.py. Gerarchia di8pagine,97simboli,336pin nella matrice. PDF,SVG e netlistXML in hardware/exports/. Il controllo di connettività verifica273assegnazioni esportate e vincoliUSB/CAN/GPIOmemoria. Non è un test di circuito.

ERC effettivo:36errori e2warning aperti, descritti in hardware/ERC_REVIEW.md.31pinFPC non assegnati;5power pin senza driverERC;2ingressi presenza non implementati. Non introdotte derogheERC per dichiarare il circuito valido. USBOVP/inrush è una funzione serie mancante, non un collegamento da ponticellare. Wake/protezioni/valoriTBD e pinout pannello impediscono uno schema definitivo o costruibile.

La richiesta del proprietario cambia l’ordine di lavoro rispetto alle stop condition originali: esiste uno schema ingegneristico modificabile e reviewable mentre i blocker restano aperti. Nessuna provaHW, conformità o peer-review indipendente dichiarata. Il filePCB vuoto creato automaticamente dalMCP è stato rimosso: non è stato eseguito layout.

## Screening successivo del buck 5 V

docs/hardware/BUCK5_FEEDBACK_REVIEW.md confronta due reti Type-3 mediante 16 punti e 64 combinazioni di tolleranze per punto, con equazioni E04 e vincoli di stabilità E52. Il circuito studiato per circa 20 mV di ripple a 12 V scende sotto 12 mV vicino a 6 V al buck. Una seconda rete conserva il margine nel modello ideale, introducendo maggiore errore DC. Nessuna rete qualificata né MPN nuovo selezionato: R210/C208/C209 restano TBD. Aggiornate le note del foglio 02 e il pacchetto di revisione; lo screening non cambia lo stato K0 né chiude il rischio di wake/fault recovery.

## Avvio software parallelo e prima revisione separata

DEC-010 autorizza app e firmware in parallelo, con assegnazioni in docs/project/WORKSTREAMS.md e contratto di sviluppo in shared/. La review documentale separata in hardware/reviews/A0_INDEPENDENT_REVIEW.md ha confrontato 53 collegamenti critici senza trovare nuove inversioni nei pin verificati. Conferma gli OPEN e precisa che EN/nSTB a zero mantiene il TCAN in Standby con INH attivo; aggiunge il rilievo condizionale GPIO3/JTAG prima di eventuale provisioning. Stato della review: limitata e documentale, non approvazione completa. I risultati software non dimostrano standby, IGN, recovery o conformità del circuito.

## Secondo incremento parallelo: studio USB e wake

`hardware/usb_power/HW02_REVIEW.md` confronta due OVP/corrente reali con disponibilità osservata e respinge il loro uso diretto davanti al TPS2116 come protezione completa: margini statici troppo stretti e risposta dinamica non garantita. Studiato il preregolatore USB candidato4,2–4,5V H, con limiti di dropout, corrente/CC e avvio ancora aperti. Tre alternative per il recupero dal fault SLEEP sono confrontate senza dichiarare100µA garantiti. Calcoli e fonti sono riproducibili con `scripts/analyze_usb_power.py`; HR-01/HR-02 restano aperti e A0 invariato. App, decoder firmware e codec di sviluppo sono avanzati su host; nessun risultato software chiude le prove elettriche.

## DEC-011 e terzo incremento

PR-020 è aggiornato a N client Bluetooth utilizzabili in momenti diversi, una sola connessione attiva; nessun reset per cambiare telefono. Il reset fisico USB/pulsanti resta manutenzione separata senza cancellazione cloud. Tracciabilità rigenerata a requisitiv0.2.6. Core sessioni/reset verificato su host e app storici locale implementata; nessuna autenticazione o radio reale abilitata.

Lo studio separato `hardware/usb_power/design_v01/` contiene un preregolatore H3,792V con14simboli, netlist/ERC e calcoli3,8/4,0/4,2/4,5V. ERC0vale solo per la connettività di quel blocco con alimentazione di banco dichiarata. Il preregolatore non costituisce da solo la protezione USB: soft-start, faultFB, dropout a bassoVIN, correnteCC e componenti da sostituire/verificare mantengono HR-02 aperto. A0 resta invariato, con36errori e2warning; nessun PCB o prova elettrica.
