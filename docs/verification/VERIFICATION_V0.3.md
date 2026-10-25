# Verifica del terzo incremento — più client BLE

24 ottobre 2026. DEC-011 aggiorna PR-020: N dispositivi utilizzabili in sequenza e una sola connessione Bluetooth attiva. Nessun owner unico permanente, reset o cancellazione necessari per cambiare telefono. Il reset USB/pulsanti approvato rimane manutenzione distinta, senza cancellazione cloud. Requisiti risultantiv0.2.6; la SPEC originaria è conservata con addendum.

## Verifica integrata

Il coordinatore ha eseguito `python3 scripts/check_software.py`:30test Python,32test Swift e3/3suiteCTest completati senza errori; build iOS Simulator senza firma riuscita. C11 e codecC sono compilati con AddressSanitizer/UndefinedBehaviorSanitizer. Nessuna build ESP-IDF, radio o dispositivo reale coinvolti. `scripts/check_k0.py` verifica la tracciabilità aggiornata, non il superamento K0.

## App e storici

Il core Swift importa pagine in un archivio file locale atomico, con deduplica device/sessione/sequenza, gap espliciti, snapshot conservato su errore e ACK solo dopo scrittura riuscita. Otto scenari JSON comuni a Python/Swift verificano resume con riapertura, conflitti, perdite, sessioni separate e interi unsigned64 anche oltre2^53/fino aUInt64.max. Il tempo resta relativo al boot.

La schermata Storici usa fixture realmente importate nel file locale, con record/stati/perdite/cursori. Restano dati dimostrativi, senza Bluetooth, cloud o timeline assoluta inventata. Limiti provvisori dell'archivio:512KiB,1024record,16sessioni e128intervalli; capacità piena rifiuta una pagina senza cancellare dati o inviare un nuovo ACK. Questi sono limiti dello scaffold storico, non un Nmassimo di telefoni consentiti.

Il riferimento `log_source.py` conserva fino a64record volatili H e produce pagine fino a16record/4096byte. ACKdiA non elimina record disponibili perB; ogni telefono mantiene il proprio cursore e le perdite sono limitate alle sequenze successive al cursore richiesto. La review separata non ha individuato nuovi bug nei percorsi esaminati; test e verifica degli estremi uint64 superati. Non è la partizione flash da2MiB.

## Sessioni e reset

Il coreC `ag_connection` accetta un solo collegamento logico; un secondo ottiene busy. Dopo la disconnessione, Bpuò entrare e poi può tornareA, senza reset. Un nuovo lease boot+contatore impedisce che un callback tardivo della vecchia sessione modifichi quella corrente. La verifica della sessione riparte da zero; il verificatore reale non è implementato e l'HAL attuale rifiuta l'autenticazione.

Il reset separato richiede evento locale qualificato, USB valida e conferma fisica. Journal, cancellazioni e commit finale sono modellati con callback di test;18punti d'interruzione I/O verificano stato incompleto bloccato e operazioni idempotenti. Prima dell'intent durevole non avviene erase; journal corrotto/incompleto o restore fallito bloccano il boot BLE logico. Le cache delle credenziali sono parte del contratto di cancellazione. Vecchi lease restano invalidi anche dopo reset completo nello stesso boot.

CRC e lease non costituiscono autenticazione crittografica o protezione da rollback fisico. Il futuro stack deve configurare un solo link, serializzare gli eventi e ripristinare il journal prima di pubblicizzare BLE. Nessun dato reale, chiave o eFuse è stato cancellato/modificato.

## Hardware separato

Il preregolatore USB di studio H3,792V ha14simboli, netlist controllata e0riscontri ERC nel report completo CLI. È stato visualizzato anche mediante MCP. Il controllo riguarda un singolo blocco con sorgente esterna qualificata dichiarata; non è collegato al mux/A0 e non è una USB completa.

Il confronto3,8–4,5V esplicita il compromesso fra i margini dei due buck. Restano aperti boundRHS a bassoVIN, curveCeff, sourcing di alcuni candidati, soft-start/inrush, OVP/corrente/CC, faultFB e corrente inversa. Il solo soft-start tipico può richiedere1,1A di carica per44µF nominali. Nessuna simulazione SPICE o prova elettrica eseguita; A0 e i suoi36errori/2warning non cambiano.
