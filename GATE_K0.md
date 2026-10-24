# Gate K0 — revisione 0.1, 2026-10-24

**Esito: NON SUPERATO / BLOCCATO.** Questo è il primo rapporto di fattibilità, non un design freeze. Il lavoro autorizzato parte da K0; K1 richiede successiva approvazione esplicita. Nessuno schematico finale, PCB, routing o ordine avviato.

Base requisiti: SPEC_V0.2.md + DEC-001 in DECISIONS.md (v0.2.1). La decisione owner chiude l'interpretazione O-02: diametro esterno massimo 44,45 mm. Non cambia PR-009 né autorizza una tolleranza positiva.

## Esito degli otto task correnti

| Task | Stato | Risultato / documento |
|---|---|---|
| 1 Tracciabilità | COMPLETATO come registro iniziale | traceability.md: 26 PR, 25 O, 12 derivati locali. Nessuna validazione hardware dichiarata |
| 2 Pannello nudo | BLOCCATO | Datasheet originale acquisito e tavole ispezionate; candidato respinto per fit. Documentazione e approvvigionamento da risolvere |
| 3 BOM candidata | PARZIALE | BOM_CANDIDATES.csv: candidati e righe TBD; stock/prezzi solo dove supportati da fonte, nessun MPN congelato |
| 4 CAN/cavo | PARZIALE | Strategia e budget illustrativi in CAN_POWER_TEST_PLAN.md; stack conforme, connettori e ramo2m non verificati |
| 5 Automotive12V/wake | BLOCCATO | Profili e target proposti, nessun dimensionamento completo né misura. O-04/O-06/O-07/O-18 aperte |
| 6 Meccanica | BLOCCATO | MECHANICAL_K0.md: due disposizioni respinte con candidato attuale; disegni quotati completi non prodotti |
| 7 Fornitori/costo | PARZIALE | Fonti pubbliche JLCPCB/PCBWay/Seeed verificate; SUPPLIER_RFQ_DRAFT.md pronta, non inviata. Nessuna conferma manual-assembly/sourcing specifica né preventivo |
| 8 Gate report | COMPLETATO come report preliminare | Questo file e issues/K0-001_BLOCKERS.md; exit gate non raggiunto |

## Fattibilità e numeri

Non fattibile: candidato pannello E01 dentro il limite esterno DEC-001. Il confronto dimensionale è in MECHANICAL_K0.md. Non è una prova di impossibilità universale del prodotto.

Plausibile ma non dimostrato: MCU/BLE + CAN su unico PCB, UI e OTA. ESP32-S3-WROOM-1-N16R8 ha un limite ambiente di +65 °C (E02), da confrontare con esposizione solare e O-07. Non assumere qualifica automotive del modulo o dell'intero assieme.

Non verificabile con le evidenze disponibili: riconoscimento affidabile del quadro sulla Polo, wake automatico, settimane in standby, immunity automotive, prestazioni5Hz, RF col magnete, costo40–70EUR e consegna assemblata completa.

Misurato sul prodotto: **nulla**. Dati disponibili: nominali da datasheet, pagine pubbliche di sourcing e calcoli riproducibili. Strumentazione, campioni e vettura non disponibili in questa sessione. Gli esempi di potenza, corrente e traffico non sono misure.

## Decisioni e azioni P0 ancora necessarie

| ID | Chiusura richiesta |
|---|---|
| O-01 | Proprietario: perimetro dei servizi diagnostici; proposta Service01 soltanto da approvare |
| O-04 | Proprietario: limite standby e intervallo dopo OFF; proposta100µA a12V da discutere, non fissata |
| O-06 | Proprietario: latenza wake, timeout e gestione CAN attivo a quadro OFF |
| O-14 | Proprietario: log su gauge o solo app connessa; retention e perdita ammessa per dimensionare memoria |
| O-16 | Sourcing/ingegneria: pannello realmente conforme, datasheet corretto, campione e fornitura lotto5 |
| O-17 | Fornitore: conferma scritta e prezzo per assemblaggio completo; contatto esterno ancora da autorizzare |
| O-18 | Prova HW-IGN: dimostrare comportamento non proprietario e concordare eventuale compromesso |

O-02 risolta nell'interpretazione; dettagli di tolleranza e inviluppo connettore ancora da precisare. O-07, pur classificata P1 in SPEC, blocca tecnicamente K0 per la stop condition sulle protezioni. O-05/O-19/O-21/O-22 e requisiti ambientali vanno chiusi per un freeze credibile. Il registro completo è traceability.md.

## Assunzioni vietate e limiti rispettati

Non usare devboard come pinout pannello; non allargare autonomamente il case; non ridurre il display; non equiparare CAN/tensione a IGN; non fare polling per svegliare il veicolo; non aggiungere120Ω; non garantire15PID a5Hz; non estendere il nominale a24V; non rimuovere protezioni contro transitori>24V; non dichiarare compatibilità Polo o servizi di assemblaggio dalla sola pagina marketing; non considerare ERC/DRC prova sufficiente; non ordinare.

## Piano per chiudere K0 e accedere a K1

1. Risolvere pannello conforme senza cambiare i requisiti; se non si trova, riportare evidenze e richiedere una change request esplicita. Acquisire datasheet approvato, disponibilità/prezzo/MOQ e campione.
2. Completare due studi quotati reali con pannello/PCB/FPC, presa e plug gauge, USB, magnete, ALS e antenna. Verificare fit e RF prima di congelare footprint.
3. Definire con owner O-01/O-04/O-06/O-07/O-14 e condizioni d'uso USB; fissare severità e criteri, poi chiudere power/thermal budget.
4. Eseguire piano HW-IGN prima su simulatore e poi sulla Polo dopo controlli di banco; raccogliere corrente e log con ground truth. Nessun test su vettura già eseguito.
5. Con autorizzazione al contatto, richiedere fattibilità/quote complete ai fornitori usando la bozza; ottenere conferma sourcing e montaggio manuale. Nessun ordine.
6. Aggiornare tracciabilità e BOM con evidenze e alternative verificate; chiudere blocker. Solo dopo approvazione K0 iniziare K1: rail, pin matrix, power/RF/thermal budget e BOM fissata. K2 creerà il progetto tramite KiCad MCP con gerarchia SPEC §10; K3 layout, K4 package e preventivo definitivo, K5 solo dopo approvazione produzione.

## Riproducibilità

Eseguire `python3 scripts/build_k0_registers.py` per rigenerare registro, CSV e manifest PDF; `python3 scripts/check_k0.py` per controlli documentali e calcoli. Il controllo non è ERC/DRC né validazione elettrica. Le fonti web sono datate e richiedono nuova verifica al freeze. Stato versionamento e aggiornamenti sono registrati in Git locale; nessuna pubblicazione remota.
