# Contratto di sviluppo app–gauge v0.1

Data: 24 ottobre 2026. Stato: proposta tecnica implementabile in simulazione, non protocollo BLE di produzione. SPEC_V0.2.md e DECISIONS.md restano le fonti dei requisiti; i dettagli qui scelti sono ipotesi di sviluppo modificabili.

## Confini

L'app configura il gauge, importa storici e prepara aggiornamenti; non offre una dashboard live. Il gauge funziona senza telefono. Service 01 soltanto; nessuna API accetta un servizio diagnostico arbitrario. Nessuna trasmissione CAN per risvegliare il veicolo. Associazione esclusiva, autenticazione, antireplay e distribuzione degli aggiornamenti richiedono ancora una policy verificata. I trasporti reali e la scrittura OTA restano disabilitati finché tali dipendenze non sono implementate e testate.

## Configurazione logica

`fixtures/config.valid.json` è l'esempio comune a modelli Swift e C. In questa iterazione il formato JSON è di scambio e di test host: non dichiara presente un parser JSON sul device.

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

Validare prima in una configurazione candidata separata. Un errore conserva quella attiva. Un futuro commit dovrà confermare `applied_revision` dopo persistenza atomica verificata; ACK di ricezione e ACK di applicazione sono distinti. Sizing, CRC e formato persistente ancora da implementare.

## Operazioni e stati da mantenere separati

| Operazione logica | Simulazione iniziale | Implementazione reale richiesta |
|---|---|---|
| Identity/capabilities | Identità chiaramente simulata, catalogo di esempio | Identità stabile, versioni HW/FW, schema supportato e PID realmente scoperti |
| Config validate | Validazione locale Swift e C | Stessi limiti e errori sul device |
| Config apply | Solo stato locale simulato | Sessione autenticata, revisione, commit atomico e conferma |
| Log import | Storici dimostrativi | Chunk bounded, cursor, ACK, resume, deduplica e perdita esplicita |
| OTA | Stato non disponibile | Firma/compatibilità, resume, verifica, rollback e recovery |
| Account/cloud | Non configurato | Provider, login, retention e privacy da decidere |

Non pubblicizzare un GATT utilizzabile senza implementazione. UUID, framing, limiti MTU, messaggi d'errore wire e protocollo di autenticazione non sono congelati. Il contratto transport Swift e l'HAL firmware possono esistere senza un collegamento BLE reale. La sola connessione BLE non attribuisce proprietà e non autorizza configurazione/OTA.

## Log e tempo

Buffer flash fino a 2 MiB condizionato al sizing OTA; nessuna allocazione che sottragga spazio alla recovery. Campionamento del logger fino a 1 Hz per sorgente/PID, separato dal polling/UI. La coda RAM ha capacità fissa e segnala perdite; il prototipo RAM non equivale alla persistenza flash richiesta. A pieno la futura partizione sovrascrive il dato più vecchio, con contatore e intervallo perso.

Ogni record conserva identificatore di sessione/boot, sequenza, tempo monotono, PID, sorgente ECU, stato e payload/valore coerente con il decoder. Tempo assoluto assente finché manca un'ancora affidabile; non inventare date. Gli stati unavailable/timeout/stale non sono zero né un vecchio valore presentato come attuale.

## Sicurezza CAN e hardware

Default TX bloccato. USB sola, standby e stato IGN ignoto bloccano richieste diagnostiche. Attività CAN e tensione batteria non provano quadro ON. La qualificazione IGN è un ingresso della macchina a stati, la cui acquisizione reale è ancora bloccata dalla verifica hardware. Nessun GPIO panel o controllo di potenza può essere dedotto da una fixture. Il software host non dimostra il target di 100 µA entro 5 minuti.

## Riferimenti tecnici consultati

[Apple CBCentralManager](https://developer.apple.com/documentation/CoreBluetooth/CBCentralManager), [ESP-IDF OTA](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/ota.html), [ESP-IDF sicurezza](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/security.html). Consultati il 24/10/2026. Le versioni stabili online possono evolvere: la release SDK effettiva va fissata prima della build device. Nessuna eFuse o chiave generata.
