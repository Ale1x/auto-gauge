# Verifica del quarto incremento

24 ottobre 2026. DEC-011 resta il vincolo BLE: più telefoni sequenziali, un solo link attivo, nessun reset per cambiare telefono. DEC-012/013 fissano CloudKit e identificatori Apple Development; DEC-014 approva codice locale per i nuovi telefoni e riconnessione dei riconosciuti; base requisiti v0.2.8.

## Prove integrate

Il coordinatore ha eseguito `python3 scripts/check_software.py` con esito positivo:35testPython,53testSwift,7suiteCTest e build unsigned iOS Simulator. La correzione UI finale dei token epoch è stata poi ricompilata dall’agente app, con nuovo pass dei53test.

`python3 scripts/check_host.py`: **35 test Python e7/7 CTest superati**, coreC e codec con ASan/UBSan. I10 scambi RPC golden verificano configurazione e storici attraverso lo stesso framing;6 scenari paginazione /12 pagine confrontano C, Python e due archivi SQLite. Comprendono overwrite, limiti payload4096/raw64, timeout, sequenze oltre2^53 e UINT64_MAX, ACK non distruttivi e sessione errata.

APP-04: **42 test Swift superati** e build unsigned iOS Simulator riuscita. Nessun successo derivato dal solo `received`: `applied/volatile` è distinto; perdita ACK dopo commit impone rilettura, disconnessione prima del commit cancella il pending. Archivio locale persistito prima dell’ACK; errore scrittura non conferma dati. Il trasporto è in memoria e la UI lo dichiara.

SwiftUI Pro applicata a struttura, tema, Dynamic Type e controlli. Rendering dei componenti SwiftUI reali a320/640pt, light/dark/accessibility3; contrasto misurato per i colori selezionati. Queste immagini non sono screenshot interattivi né certificazione VoiceOver: [design e limiti](../validation/ios/README.md).

## Firmware e target

FW-05 verifica24.653 confini byte di power loss in8 fasi, corruzione di ogni byte del settore4KiB, revisioni/generazioni e capability incompatibili. La selezione della revisione integra più nuova precede il controllo dei PID del veicolo: nessun rollback nascosto. Short read blocca scritture; la copia leggibile è diagnostica e non prova che il settore illeggibile fosse più vecchio.

[Review indipendente](../reviews/software/FW05_REVIEW.md) completata, senza nuovi P0/P1 riproducibili. Il modello di erase/program simulato non dimostra l’atomicità della flash reale. Occorre dimensionare e misurare lo stack del task prima dell’integrazione: i buffer di settore sono automatici e le chiamate annidate.

Build ESP32-S3 con ESP-IDFv6.1 riuscita,11 coreC compilati e `.bin`170624byte. [Report](../../firmware/device-build-report.json) e [procedura](../firmware/DEVICE_BUILD.md). Il main rimane inerte; gran parte del core viene eliminata dal linker. Factory partition2MB senza OTA/PSRAM e senza driver BLE/CAN/display operativi: questi numeri non provano il dimensionamento della V1. Nessun flash, eFuse o test radio eseguito.

## CloudKit APP-05 e trasporto FW-06

La suite Swift completa arriva a **53 test**, inclusi11 per outbox/account/cloud con database finto. Adapter nativo per accountStatus e upload privato immutabile con recordID deterministici, confronto contenuto e create condizionale; outbox atomico bounded512KiB/1024elementi/8contesti. Fixture escluse, nessun upload automatico, ACK gauge indipendente dal cloud. I contatori64bit restano stringhe. Il prototipo conserva ricevute uploaded: capacità piena rifiuta nuovi dati senza scartarli.

La review indipendente ha riprodotto un callback account obsoleto che poteva invalidare il nuovo contesto selezionato; corretto controllando generazione/contesto prima di invalidare, con regressione A→B durante await. Restano limiti espliciti: upload/riconciliazione dei singoli record, nessun download/restore completo; ricevute storiche non audit corrente del backup; cambio account durante richiesta CK già in volo da verificare fisicamente. [Review](../reviews/software/CLOUDKIT_REVIEW.md). Nessuna sync live dichiarata.

FW-06 aggiunge handoff host di Flow Control CTS/overflow per risposte Service01 multi-frame, profili fisici11/29bit, block size e STmin0–127ms. Prepare e dispatch verificano le stesse guardie CAN; perdita autorizzazione invalida la transazione. Non supporta precisione microsecondi, WAIT, CANFD o ISO-TP extended/mixed addressing. Test di sequenza/wrap, timeout, malformed e cambi delle guardie passati; nessun invio fisico o conformità ISO dichiarata. La build target aggiornata riesce e mantiene lo skeleton inerte.

## Hardware e CloudKit

`python3 scripts/design_presence_sensing.py` riprodotto dal coordinatore:35 assegnazioni pin e0 rilievi ERC, render ispezionato. [Studio HW-04](../../hardware/presence_sensing/HW04_REVIEW.md) separato da A0; fail-safe non fornisce un massimo numerico di leakage e non dimostra100µA/5min. A0 mantiene i propri blocker, nessun PCB o ordine.

La build firmata Development è stata tentata in copia temporanea con gli identificatori approvati. Xcode risponde **No Account for Team ZDQFU74347** e nessun profilo per il bundle. Il proprietario conferma di non avere un abbonamento Developer; Apple richiede membership attiva per CloudKit anche in sviluppo. Nessun abbonamento attivato, container disponibile o sync live dichiarati. Stato: [configurazione Apple](../mobile/APPLE_CONFIGURATION.md). Il codice CloudKit e i test con database finto sono un’attività separata dalla verifica live.

La V1 completa richiede ancora pannello esatto/FPC/rails, protezioni e wake/IGN verificati, firmware runtime/flash/BLE autenticato/OTA, integrazione e prove su banco. Repository e build software non chiudono K0.
