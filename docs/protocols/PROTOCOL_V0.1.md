# Contratto di sviluppo app–gauge v0.1

Data: 24 ottobre 2026. Stato: proposta tecnica implementabile in simulazione, non protocollo BLE di produzione. docs/product/SPEC_V0.2.md e docs/project/DECISIONS.md restano le fonti dei requisiti; i dettagli qui scelti sono ipotesi di sviluppo modificabili.

## Confini

L'app configura il gauge, importa storici e prepara aggiornamenti; non offre una dashboard live. Il gauge funziona senza telefono. Service 01 soltanto; nessuna API accetta un servizio diagnostico arbitrario. Nessuna trasmissione CAN per risvegliare il veicolo. DEC-011 consente più client Bluetooth in momenti diversi, con una sola connessione attiva; non esiste vincolo a un solo iPhone persistente. Autenticazione, antireplay e distribuzione degli aggiornamenti richiedono ancora una policy verificata. I trasporti reali e la scrittura OTA restano disabilitati finché tali dipendenze non sono implementate e testate.

## Configurazione logica

`fixtures/config.valid.json` è l'esempio comune a modelli Swift e C. Il parser C bounded è ora in `c/ag_config_json.c`, compilato anche con ESP-IDF ma non richiamato dal main inerte; `docs/protocols/CONFIG_JSON_C.md` documenta il sottoinsieme supportato. Il parser non autorizza operazioni sul dispositivo.

`fixtures/config.cases.json` contiene 22 casi con risultato strutturale atteso. `python3 scripts/check_shared_contract.py` li verifica, inclusi duplicati, limiti, interi veri (i booleani JSON non sono interi), rettangoli e versioni. Il test non prova che un PID sia supportato né autorizza un commit sul device.

| Campo | Regola v0.1 di sviluppo |
|---|---|
| `schema_version` | Intero esattamente 1; rifiutare altre versioni |
| `revision` | Intero positivo, fino a 2147483647; la ricezione non significa applicazione |
| `brightness.mode` | `manual` oppure `auto` |
| `brightness.manual_percent` | Intero 0–100; memorizzato anche in auto. Curva ALS e minimo effettivo sono aperti |
| `dashboards` | Da 1 a 3 elementi |
| `dashboards[].id` | Intero 1–255, univoco nell'intera configurazione |
| `dashboards[].widgets` | Da 1 a 5 elementi |
| `widgets[].id` | Intero 1–255, univoco nella dashboard |
| `widgets[].pid` | Intero 1–255, Service 01 implicito; il catalogo di capability deve inoltre confermare che sia una misura utilizzabile. La semplice validità numerica non prova supporto ECU |
| `widgets[].rect` | `x,y,width,height` interi in unità 1/10000 del riquadro del display. x,y≥0; width,height>0; x+width≤10000 e y+height≤10000 |

La griglia normalizzata è una base di sviluppo per O-10, non una decisione sul modello definitivo di editor. L'attuale validazione verifica il riquadro: clipping sul cerchio, sovrapposizioni, font e soglie restano da definire. L'anteprima usa valori dimostrativi etichettati. PID ripetuti in dashboard diverse sono consentiti; il logger deduplica per sorgente/PID, non genera polling aggiuntivo.

Validare prima in una configurazione candidata separata. Un errore conserva quella attiva. Un futuro commit dovrà confermare `applied_revision` dopo persistenza atomica verificata; ACK di ricezione e ACK di applicazione sono distinti. Il formato persistente a due slot è modellato in `firmware/include/ag_config_store.h` con CRC, marker e fault injection; backend flash reale, bootstrap e integrazione runtime restano da implementare/qualificare.

## Operazioni e stati da mantenere separati

| Operazione logica | Simulazione iniziale | Implementazione reale richiesta |
|---|---|---|
| Identity/capabilities | Identità chiaramente simulata, catalogo di esempio | Identità stabile, versioni HW/FW, schema supportato e PID realmente scoperti |
| Config validate | Validazione locale Swift e C | Stessi limiti e errori sul device |
| Config apply | Solo stato locale simulato | Sessione autenticata, revisione, commit atomico e conferma |
| Log import | Storici dimostrativi | Chunk bounded, cursor, ACK, resume, deduplica e perdita esplicita |
| OTA | Stato non disponibile | Firma/compatibilità, resume, verifica, rollback e recovery |
| Account/cloud | CloudKit scelto; adapter upload/restore implementato e testato con fakeDB | Membership, provisioning e test live; retention/privacy/cancellazione da finalizzare |

Non pubblicizzare un GATT utilizzabile senza implementazione. UUID, framing, limiti MTU, messaggi d'errore wire e protocollo di autenticazione non sono congelati. Il contratto transport Swift e l'HAL firmware possono esistere senza un collegamento BLE reale. La sola connessione BLE non attribuisce proprietà e non autorizza configurazione/OTA.

Il secondo incremento aggiunge una proposta di framing implementata nei codec di test: `docs/protocols/FRAMING_V0.1.md`, con vettori comuni Python/C/Swift, e un riferimento locale di sync in `docs/protocols/LOG_SYNC_V0.1.md`. Questi componenti non congelano GATT o sicurezza e non attivano trasporti reali. La policy corrente sui client e il reset è in `docs/protocols/CONNECTION_POLICY_V0.1.md` (DEC-011); la proposta storica `docs/protocols/OWNERSHIP_PROPOSAL.md` è superata.

## Log e tempo

Buffer flash fino a 2 MiB condizionato al sizing OTA; nessuna allocazione che sottragga spazio alla recovery. Campionamento del logger fino a 1 Hz per sorgente/PID, separato dal polling/UI. La coda RAM ha capacità fissa e segnala perdite; il prototipo RAM non equivale alla persistenza flash richiesta. A pieno la futura partizione sovrascrive il dato più vecchio, con contatore e intervallo perso.

Ogni record conserva identificatore di sessione/boot, sequenza, tempo monotono, PID, sorgente ECU, stato e payload/valore coerente con il decoder. Tempo assoluto assente finché manca un'ancora affidabile; non inventare date. Gli stati unavailable/timeout/stale non sono zero né un vecchio valore presentato come attuale.

## Sicurezza CAN e hardware

Default TX bloccato. USB sola, standby e stato IGN ignoto bloccano richieste diagnostiche. Attività CAN e tensione batteria non provano quadro ON. La qualificazione IGN è un ingresso della macchina a stati, la cui acquisizione reale è ancora bloccata dalla verifica hardware. Nessun GPIO panel o controllo di potenza può essere dedotto da una fixture. Il software host non dimostra il target di 100 µA entro 5 minuti.

## Riferimenti tecnici consultati

[Apple CBCentralManager](https://developer.apple.com/documentation/CoreBluetooth/CBCentralManager), [ESP-IDF OTA](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/ota.html), [ESP-IDF sicurezza](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/security.html). Consultati il 24/10/2026. Le versioni stabili online possono evolvere: la release SDK effettiva va fissata prima della build device. Nessuna eFuse o chiave generata.

## Stato implementazioni successivo

La policy UX dei nuovi telefoni è approvata in DEC-014; controller C in `ag_admission`, senza binding radio qualificato. Profilo GATT candidato in `docs/protocols/GATT_V0.1.md`/`gatt.profile.json`: adapter CoreBluetooth opt-in e solohello, con mutazioni native bloccate. Codec C request/reply in `docs/protocols/RPC_C_V0.1.md`, stream in `docs/protocols/C_STREAM_V0.1.md`; nessuno dei codec è un dispatcher autorizzato.

Storico persistente COW simulato in `docs/firmware/FLASH_HISTORY_MODEL.md`: recovery e sessioni testate, ma costoO(capacità) e bloccoREAD_ONLY dopo banco sporco lo rendono inidoneo al backendV1. Verifica pacchetto OTA firmato offline in `docs/protocols/OTA_V0.1.md`: non installa immagini o programma eFuse. Questi avanzamenti non abilitano HAL/CAN/display né dimostrano standby o autonomia reale.
