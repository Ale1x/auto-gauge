# Sesto incremento: integrazione software

24 ottobre 2026. Requisiti v0.2.8 e decisioni BLE invariati: più telefoni riconosciuti in sequenza, una sola connessione, codice locale solo per nuovi telefoni. Nessuna membership Apple, radio, flash o eFuse attivati.

## Risultati

**APP-08:** editor e storici della demo usano lo stesso client RPC framed. I test macOS collegano i byte originali al processo C reale, senza aggiungere campi alle risposte: configurazione su file, riavvio, A–B–A, conflitto revisioni, ACK perso, errore dopo marker e import locale prima dell’ACK. Le risposte diagnostiche sono tipizzate; un archivio degradato non diventa current e non risolve un commit ambiguo. Il launcher di processo è soltanto nei test. La UI conserva la demo esplicita e il percorso CoreBluetooth hello-only. [Review mobile](../reviews/software/APP08_REVIEW.md).

**Endpoint C:** stream, ammissione, RPC, store e sorgente storici bounded collegati in un dispatcher senza heap. Received precede il commit; applied richiede commit e rilettura riusciti. Lease obsoleti non modificano il peer corrente. Configurazione con slot sporco/corrotto impone diagnosi READ_ONLY, senza fallback operativo silenzioso. CLI con pwrite/fsync soltanto host; nessuna equiparazione alla flash del gauge. [Contratto](../firmware/DEVICE_ENDPOINT.md), [review del coordinatore](../reviews/software/ENDPOINT_INTEGRATION_REVIEW.md).

**Binding NimBLE isolato:** servizio generato dal profilo condiviso, un link, requisiti Secure Connections/MITM/128bit e verifica bond, finestra locale per nuovi telefoni, frammentazione indicate e generazioni dei callback. Il bridge collega il controller di ammissione all’endpoint e aspetta EDONE di tutto received prima del commit. Errori task/timer/close e risposte scadute sono coperti da fault injection. SDK v6.1 compila e collega tutte le nove entry point pubbliche; immagine448880byte, main vuoto. **Non eseguito né pronto da flashare.** Otto bond e stack32KiB sono limiti dello studio, non capacità N o dimensionamento finale. [Studio e riproduzione](../../firmware/ble_study/README.md), [review e limiti](../reviews/software/NIMBLE_STUDY_REVIEW.md).

**FW-09:**53 PID scalari Service01 con formule razionali, lunghezze esatte e guardia TX coerente. Copertura min/max, punti intermedi e bitmap documentata con fonti nel [catalogo](../firmware/DIAGNOSTIC_COVERAGE.md). Non implica supporto di tutti i PID standard o disponibilità su una specifica auto.

## Verifiche e limiti

Il coordinatore ha completato `python3 scripts/check_software.py`: **71 test Python,105 test Swift e11/11 CTest superati**, build iOS Simulator unsigned riuscita. Anche la build iOS generic device unsigned è riuscita nel filone app. I test host includono ASan/UBSan, processi C/Swift e harness con shim SDK; gli shim non qualificano ABI, crittografia, SMP/ATT o timing reali. La build SDK separata verifica compilazione/link. Progetto Xcode e tre risorse JSON rigenerati identici; check K0 coerente, gate hardware ancora aperto. Build principale ESP32-S3:15 sorgenti core e4 condivise, main inerte; studio BLE separato con hash sorgenti finali verificati.

Il prodotto software resta allo **step4, integrazione operativa**: il percorso host è verificabile, quello fisico deve ancora essere attivato e qualificato. Seguono runtime autonomo, backend storico efficiente e recupero dopo interruzione, diagnostica restante, OTA/recovery target, CloudKit live quando sarà disponibile la membership e QA su dispositivi. Lo storico COW precedente usa metà spazio e O(capacità) per append: non soddisfa ancora il backend V1. Nessuna nuova decisione richiesta per questo incremento.
