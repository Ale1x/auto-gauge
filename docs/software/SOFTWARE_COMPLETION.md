# Completamento software — inventario eseguibile

24 ottobre 2026, baseline pubblicata V0.11 (`c6a562e`). Richiesta corrente: completare il software prima delle istruzioni per prove su due ESP32. Le due schede non sono ancora identificate e non si esegue flash. Questo inventario distingue codice ancora da scrivere, policy/configurazione da fornire e prove fisiche; nessuna voce è chiusa per il solo esito della CI.

## Stato corrente — V0.13, 25 ottobre 2026

La nuova richiesta autorizza di proseguire autonomamente il software mentre il proprietario è assente. Sono ora implementati recovery archivio/inbox APP15 con intenti nello stesso commit, interop catalogo Swift→C, flusso OTA APP16 con progresso/annulla e select separata, owner applicativo, renderer RGB565, provisioning blank-only e journal reset NVS. Il bridge condivide admission/RPC dell'app e sceglie localmente una route OTA esclusiva; il target nativo collega insieme gli adapter reali di storage/TWAI/NimBLE/OTA/boot. Nessun profilo scheda o trust di produzione è inventato: il getter locale predefinito restituisce NULL e rifiuta l'avvio prima dell'IO.

Il vecchio crash window APP15 è chiuso per i nuovi import qualificati: record/cursore/intenti condividono una sostituzione atomica e recovery idempotente. Vecchi archivi v1 non acquisiscono retroattivamente provenienza qualificata. Il gate nativo app resta negato senza provider locale; la selezione OTA dall'esito incerto resta terminale.

Restano dipendenze concrete: dati delle due schede e profilo elettrico/pannello/IGN, tempi e salute quantitativi qualificati, trust/chiavi/rotazione produzione, lifecycle per cancellare credenziali persistenti e cache host/controller preservando reset/trust/floor (audit `docs/reviews/software/BLE_CREDENTIAL_RESET_AUDIT.md`), semantica dei PID standard oltre i53scalari e relativo modello multicomponente. Membership/provisioning CloudKit e prove fisiche sono esterni; nessuna spesa o operazione su dispositivo è autorizzata da questa fase. Non dichiarare «tutto il software/prodotto completato» finché queste integrazioni applicabili non sono risolte.

I risultati e i comandi di consegna saranno riportati in `docs/verification/VERIFICATION_V0.13.md` e `docs/software/SOFTWARE_DELIVERY.md`. Le sezioni seguenti sono una cronologia V0.11/V0.12, non una lista di funzionalità tutte ancora assenti.

## Stato storico della baseline V0.11

Il core non è un firmware operativo: `firmware/idf/main/app_main.c` inizializza un SID e verifica il blocco CAN, poi attende; `hal_unimplemented.c` restituisce NOT_IMPLEMENTED/BLOCKED per power, display, log, BLE, OTA e reset. Gli studi BLE/OTA hanno main vuoti. L'app ha editor/storici e CloudKit predisposti, ma `AppModel` istanzia esclusivamente `DevelopmentGaugeTransport`/fixture; il controller CoreBluetooth espone soltanto hello. I moduli OTA Swift non sono chiamati dalla UI `FirmwareView`, che mostra un pulsante disabilitato. Non si può descrivere questa situazione come «tutto il software completato».

## Backlog ordinato, con criterio di chiusura locale

| Ordine | Lavoro software concretamente mancante | Evidenza attuale | Risultato da completare prima del banco |
|---|---|---|---|
| 1 | Stato OTA persistente, salute e floor coordinati con rollback | `ota_installer` non salva release/health; `ag_manifest_trust_t.accepted_release_floor` arriva dal chiamante; nessuna conferma salute/reboot | Journal versionato con IO iniettabile, pending candidato legato a hash/slot/release, recovery dopo ogni confine IO e chiamata SDK, health policy esplicita, floor avanzato soltanto nell'ordine verificato. Adapter ESP-IDF/NVS compilato con vere API e test di fault; nessuna eFuse. |
| 2 | Backend partizioni reali e manutenzione/reset durabile | Config store/journal usano callback; `ag_hal_log_persist` e reset HAL assenti | Adapter partition read/program/erase con geometria/bounds/readback, labels e layout di sviluppo espliciti; provisioning iniziale separato da open/recovery; bootstrap senza cancellazione automatica. Coordinare journal reset, bond/config/log con interruzioni. |
| 3 | Proprietario applicativo unico e avvio coerente | `runtime.c` non è invocato da main; BLE e OTA sono studi separati | Controller di boot e ciclo eventi che compone store, discovery, runtime, endpoint RPC, OTA e manutenzione con stati esclusivi e clock fresco dopo IO; profilo banco con periferiche simulate dichiarate e profilo prodotto senza pin inventati. Build che trattiene e usa realmente i moduli; risorse/task configurati e diagnostica bounded. |
| 4 | Discovery CAN e piano iniziale capability-aware | `ag_obd_capabilities_apply` e builder esistono, ma le capability/job runtime sono forniti dalle fixture; PR-026 resta aperto | Orchestratore bounded dei profili standard/page bitmap/ECU, timeout/abort/revoca e selezione dei soli PID effettivamente utilizzabili; builder dei default da una policy esplicita, senza imporre metriche/count/layout non approvati. Adapter TWAI con guardie immediatamente prima del driver, completamento TX e bus-off. |
| 5 | Gauge UI e controllo locale autonomo | Nessun renderer/display/touch/ALS/power operativo | Scene/model bounded alimentati da config e misure, `—` per qualità non valida, tap per ciclo dashboard, luminosità manuale/auto con policy iniettabile, stato pairing/manutenzione/OTA e recovery locale. Backend display/touch/power collegabili tramite board profile; test logici senza assegnare pin/FPC/curve IGN ipotetici. |
| 6 | Route BLE completa e sessioni native app | Bridge NimBLE inoltra solo JSON RPC; app nativa solo hello; OTA framed dedicato non instradato | Router locale RPC/OTA esclusivo, manutenzione autorizzata sul target e cleanup su disconnect/revoca/confirm fallito. Unire config/history native all'app mantenendo revisioni/unknown/import-prima-ACK; più telefoni sequenziali senza reset. Gate di produzione basato sullo stack/policy qualificati, mai su `hello.authenticated`. |
| 7 | Flusso OTA utente e gestione pacchetti | Verifier/sender/packet transport testati; `FirmwareView` è un placeholder | Import bounded di envelope/immagine, trusted release context locale separato dal pacchetto, preparazione fuori MainActor, progresso/annulla e selezione distinta, messaggi italiani per UNKNOWN e nessun retry cieco. Distribuzione/download e trust provisioning configurabili senza creare una chiave prodotto o spendere denaro. |
| 8 | Copertura diagnostica V1 e limiti dei log | `docs/firmware/DIAGNOSTIC_COVERAGE.md`: 53 PID scalari, non tutti i Service01 standard; runtime raw≤8, storico raw≤64; journal può READ_ONLY dopo tagli e query O(capacità) | Matrice esplicita dei PID standard scalari/composti/enum richiesti e rappresentazione dati tipizzata, decoder e fixture/fonti verificabili; nessun PID OEM/DTC/write. Budget e indice lettura/log scheduling per non bloccare acquisizione/radio; recovery degradata separata dalla disponibilità del gauge. Non dichiarare completa PR-003 con l'allowlist53. |
| 9 | Flusso storici reali/cloud e gestione dati | Cloud outbox/restore esistono e sono testati con fakeDB; `AppModel` importa solo fixture simulate, escluse dal cloud | Collegare import reale verificato all'outbox dell'account corrente, mantenendo separazione delle fixture; UI errori/offline/account change/resume, export locale e cancellazioni esplicite secondo policy. Capability CloudKit e collaudo live rimangono una fase esterna. |
| 10 | Pacchetto di consegna per il banco | Studi/build isolati, niente profilo completo per le schede dell'utente | Comando unico riproducibile, configurazioni/pin/memoria per board dichiarate, manifest artefatti/hash/provenienza, UI e procedure di test coerenti, diagnostica senza segreti, reset/recovery documentati. Solo dopo identificazione delle due schede si sceglie il target compilato e il piano di flash fornito dall'utente. |

Le prime cinque voci possono avanzare senza membership Apple o dispositivi connessi. Le voci 6–9 hanno parti implementabili interamente su host; il loro collegamento a radio, flash, display e account reali resta esplicitamente non validato fino alle prove pertinenti. Un test con due ESP32 non può verificare da solo pannello AMOLED, assorbimento automotive/IGN, alimentazione protetta o veicolo.

## Decisioni già fissate da conservare

DEC-003: solo Service01 live read-only; DEC-006:2MiB se disponibili dopo sizing OTA,≤1Hz globale/PID e overwrite con perdita esplicita; DEC-011/014: N telefoni sequenziali, un solo link e passkey locale soltanto per nuovi telefoni; DEC-012/013: iCloud/CloudKit e identificatori Apple già approvati; DEC-008: reboot al crank ammesso con ripartenza automatica. Nessun acquisto/membership, App Store, ordine, contatto fornitore, flash o eFuse discende dal completamento software corrente.

Le policy ancora aperte non vanno trasformate in false decisioni: O-10 layout/editor, O-11 cambio veicolo, O-12 curva/precedenze ALS, O-15 distribuzione/provisioning/recovery, O-24 default iniziali e condizioni quantitative di salute. Si possono implementare meccanismi parametrizzati con profili di sviluppo dichiarati e validati, lasciando esplicita la configurazione prodotto necessaria. Il modello delle schede, memoria/PSRAM e periferiche non si deducono dalla sola parola «ESP32».

## Documentazione storica da riconciliare alla chiusura

- La tabella iniziale di `firmware/README.md` afferma ancora parser non integrato, sette decoder e cinque CTest; successive sezioni e V0.11 documentano dispatcher,53decoder e18CTest. Riassunto iniziale e «Prossimi incrementi» non sono lo stato corrente.
- `docs/project/traceability.md` contiene stati ancora generici «DA IMPLEMENTARE» su autonomia/OTA e riferimenti editor storici; O-23 descrive iOS/login da decidere mentre iOS17 e CloudKit sono già adottati. Aggiornare evidenze/stato senza promuovere test host a qualifica.
- `docs/archive/handoffs/HANDOFF_2026-10-24.md` è uno snapshot V0.8/V0.9, non il backlog corrente: conserva intenzionalmente SHA, build e incrementi allora non pubblicati.
- `docs/product/SPEC_V0.2.md` è storico/normativo con addenda in `docs/project/DECISIONS.md`: non cancellare la cronologia o ripristinare ownership unica,7ore di retention, supporto24V o O-01 servizi estesi.

## Criterio di uscita

Il coordinatore può dichiarare «implementazione software pronta per il banco» solo quando il backlog applicabile è collegato al profilo di esecuzione, i flussi utente sono raggiungibili, bootstrap/recovery/fault hanno esiti documentati e gli artefatti sono riproducibili. Le restanti voci esterne devono essere nominate con la loro dipendenza concreta. «Prodotto completo/validato» richiede inoltre collaudo fisico e CloudKit live: questi risultati non sono sostituibili con numero di test, link SDK o binario piccolo da linker che scarta core inutilizzato.

## Aggiornamento di chiusura —25ottobre2026, V0.12

La sezione precedente fotografa la baselineV0.11. Sono ora implementati e verificati: stato boot/floor con witness locale e NVS, factory bootstrap esplicito offdi default, adapterpartizioni, discovery/default, controllerUI gauge, TWAI moderno con completion, catalogo `logs.sessions` e scannerjournal, runtime live-only opzionale dopo faultstorage, preparazione OTA locale/UI e dependency injection delle sessioni native. Verifiche e limiti aggiornati in [VERIFICATION_V0.12](../verification/VERIFICATION_V0.12.md).

Non sono ancora composti in un unico firmware applicativo/boardprofile. Restano reset durabile/provisioning e raccolta salute, route OTA autorizzata nel bridgeBLE, renderer e HAL fisiche, qualifica del provider nativo, copertura Service01 oltre53scalari, manutenzione journal e pacchetto per le schede. APP15 è predisposta con gate chiuso: risolvere la transazione/recovery archivio→inboxcloud prima di qualificarla per dati reali e aggiungere interop catalogo Swift→C. Nessun ordine, membership o flash è stato eseguito.

La richiesta «completa tutto» rimane l’obiettivo generale; l’ultimo messaggio chiede di chiudere gli step in corso e consegnare un handoff pulito. Nessun completamento del prodotto viene dichiarato. Le due ESP32 dell’utente non sono identificate; userà una fase successiva per indicare i test.

## Aggiornamento piattaforma mobile — 25 ottobre 2026

DEC-015 porta il target minimo a iOS27.0, aggiornando app, package e generatore. I riferimenti iOS17 nelle baseline e nei report V0.1…V0.13 sono storici. Verifiche della migrazione in `docs/mobile/IOS27_MIGRATION.md`.
