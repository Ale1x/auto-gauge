# Logging locale limitato — proposta K0, 2026-10-24

## Requisito approvato e dettagli ancora aperti

DEC-004 approva un buffer sul gauge con sync successiva BLE e impone di non sovraccaricare il dispositivo. DEC-006 approva2MiB fissi se disponibili dopo sizingOTA, fino a1Hz perPID e sovrascrittura dei dati più vecchi con segnalazione della perdita. La durata conservata non è garantita; formato e prestazioni da verificare. Gli altri valori H sotto servono a rendere la scelta verificabile; non sono un'implementazione firmware.

## Architettura proposta

- Partizione dati di dimensione fissa, approvata **2 MiB condizionati al sizing OTA [DEC-006]**, separata dagli slot OTA e dalla configurazione. Mai estendere automaticamente la partizione né usare lo spazio dell'immagine di recovery.
- Registro circolare append-only, sequenza e checksum per blocco. Policy approvata a pieno: sovrascrivere i blocchi più vecchi e registrare la perdita con contatori e intervallo perso; approvata DEC-006. Niente riempimento illimitato.
- Coda RAM fissa, candidata **64 record × 16 byte = 1 KiB [H]**, con metadati e strutture del driver da aggiungere al budget. Se la coda è piena, segnalare drop; la UI e il task CAN non aspettano il logger.
- Scrittura in blocchi e task a priorità inferiore a CAN/UI. Non scrivere flash da interrupt o callback CAN/BLE. Pre-erase pianificato e buffer in RAM: la bassa priorità da sola non impedisce stalli di cache/flash, da misurare.
- Registrare solo i PID configurati; fino a **1 Hz per PID [DEC-006]**, indipendente dalla lettura/UI a 5 Hz. La decimazione non deve generare richieste CAN aggiuntive. Raw frame, flash erase e trasferimenti BLE non sono servizi diagnostici aggiuntivi.
- Sync BLE a chunk limitati con numero di sequenza, verifica integrità, ACK persistente e resume. La disconnessione non duplica i record importati nell'app. Con carico CAN/UI o durante OTA, ridurre o sospendere il sync secondo policy da fissare.
- Alla richiesta di standby, fermare il logger entro il termine complessivo di 5 minuti, completando solo operazioni bounded; non tenere alimentato il gauge per svuotare lo storico. Non pubblicizzare BLE per effettuare sync in standby senza riesame del budget.

Il record compatto da 16 byte è solo modello di dimensionamento: deve includere PID/service implicito o esplicito, origine ECU, tempo monotono, valore/raw e stato. Il formato finale deve conservare i dati necessari senza troncare risposte o perdere la semantica del decoder. Nessun valore fittizio per TIMEOUT/STALE.

## Durata teorica con 2 MiB interamente dati

| Carico ipotetico | Payload | Durata superiore teorica |
|---|---:|---:|
| 5 PID × 1 Hz × 16 byte | 80 byte/s | 7,28 ore |
| 15 PID × 1 Hz × 16 byte | 240 byte/s | 2,43 ore |
| 5 PID × 5 Hz × 16 byte | 400 byte/s | 1,46 ore |
| 15 PID × 5 Hz × 16 byte | 1.200 byte/s | 0,49 ore |

Header, indici, allineamento, checksum e blocchi di recupero riducono le durate. Nessuna compressione assunta. La disponibilità effettiva di 2 MiB richiede prima il sizing firmware/OTA/asset. Il firmware non è ancora compilato e il modello non garantisce ore di storico.

## Tempo, power loss e usura

Ogni boot deve avere identificatore sessione e timestamp monotono; ancorare al tempo iPhone quando disponibile. Senza RTC/timebase confermati, l'app deve distinguere un orario assoluto ignoto da quello realmente noto. Non inventare date per la guida senza telefono.

Recovery tramite record/blocchi validi con checksum e commit marker; metadati ridondanti, nessuna scansione non limitata durante boot. Testare spegnimenti durante append, erase e sync. La flash del modulo esatto deve avere part number e geometria/endurance verificati: nessun numero di cicli assunto.

A 5 PID/1 Hz, 8 ore/giorno sono 2,304 MB/giorno di solo payload, circa 1,10 giri/giorno della partizione da 2 MiB. A 15 PID/5 Hz sono 34,56 MB/giorno, circa 16,48 giri/giorno. Scritture metadati e write amplification possono peggiorare l'usura; non è una previsione di vita utile.

## Prove richieste

Misurare rate PID effettivo, frame UI e latenza con logger spento/acceso, erase, sync BLE, buffer pieno e OTA; zero attese del logger nei percorsi critici, soglie numeriche di prestazione da fissare in O-09. Verificare memoria massima costante, drop espliciti, recupero power loss, resume/deduplica e limite standby. T-016 rimane NON ESEGUITO. Nessun componente storage aggiuntivo selezionato prima di queste scelte.

Approfondimento quantitativo riproducibile in CAPACITY_K0.md e evidence/models/: allocazione OTA ipotetica, overhead/formato log e latenza diagnostica. Non sostituisce build o prove.
