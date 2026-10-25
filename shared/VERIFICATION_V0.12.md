# Dodicesimo incremento: chiusura dei filoni avviati

25ottobre2026. Requisiti v0.2.8/iOS17 invariati. Nessun flash, eFuse, acquisto, membership, messaggio a fornitori, radio/device/veicolo o cloud live. L’utente ha dueESP32 e darà in seguito le istruzioni dei test; il modello delle schede è sconosciuto. Questa chiusura precede una sessione pulita, non dichiara tutto il prodotto/software completato.

## Risultati implementati

- Boot OTA: record96byte versionato con floor, candidato/hash/slot e witnesshealth; ordine intent→healthpersistita→SDKVALID→floor, riconciliazione dopo errori che possono seguire commit. NVSbind non crea namespace; provisioning separato. Select richiede callback locale durevole, NULL rifiuta. Factory→OTA0 solo con flaglocale esplicito offdi default, preservando fallback. Nessuna eFuse. [Contratto](OTA_BOOT_HEALTH_V0.1.md), [review](../reviews/BOOT_HEALTH_REVIEW.md).
- Storage: adapterESPpartition per config8KiB/journaldev256KiB, metadata/geometry/all-partition exclusion, 0→1precheck e readback, latch scritture dopo fault. Nessun format automatico. [Studio](../firmware/platform_study/README.md).
- Discovery:8ECU/page bounded sulle4varianti CAN fornite localmente; capability e default soltanto dai53PID scalari realmente usabili, nessun auto-bitrate/bootstrapflash. UIgauge portabile:≤5widget,53decoder raw, qualità/stale/zero, tap/dashboard/revisioni, power/ALS espliciti. [Discovery](../docs/DISCOVERY.md), [UI](../docs/GAUGE_UI.md).
- TWAI: driver modernoSDK con ISRqueue16, Service01/FC soltanto, guardie pre/post, QUEUED distinto da completamento, errore/timeout/busoff terminali senza recovery automatico. Corretto il timer di risposta: parte dall’osservazione TXcomplete, non enqueue; FC non lo rinnova. [Contratto](../docs/TWAI_PORT.md).
- Runtime: policy locale opt-in prima dello start consente live polling dopo faultstorage disabilitando definitivamente logging, senza retry o persistenza inventata. Defaultstrict invariato; not_recorded è diagnosticaRAM, non lossrange durabile.
- Storici: nuovo RPCreadonly `logs.sessions`, schema/ID/cursor tipizzati bounded16; catalogo journal ordinato, include sessioni vuote leggibili, noerase/ACK/stateclient. Non-snapshot, queryO(capacità). [Contratto](HISTORY_SESSIONS_V0.1.md).
- APP14: fileimport bounded/security-scoped, snapshot, verifica fuoriMainActor, archive atomico senza trust o verifiedflag, riapertura con verifica corrente. UI italiana e screenshot light/dark. Corrette staleverification su archivio assente e serializzazione cancel/restore. [Review](../reviews/PREPARATION_AND_CATALOG_REVIEW.md).
- APP15: AppModel con binding simulato/nativo e bozze separate, nativeRPC con providerlocale per generazione/deviceID, gate reale predefinito negato; catalogo sessioni, archive-beforeACK, receipt sigillati e inboxlocale→outbox. Corretto un cleanup tardivoA che poteva toccareB; due regressioni. Non deriva qualifica da hello. Il crashwindow fra filearchivio e inbox rimane aperto e blocca l’abilitazione per dati reali. [Reviewcoordinatore](../reviews/COMPLETION_COORDINATOR_REVIEW.md).

## Verifiche finali

`python3 scripts/check_software.py` ha passato107Python,173Swift,23/23CTest con sanitizzatoriC e buildsimunsigned. Dopo le due regressioni di lease native, suiteSwift intera **175test** passati e build iOSsim+genericdevice unsigned ripetute:exit0. Dopo la correzione TWAI ripetuto il gruppo interessato7casi e crossbuild reale. Nessuna suite fallita è nascosta da questi conteggi.

Gruppi nuovi: bootcore8, bootreview3+bindingSDK4, factoryinstaller+8baseline, storage4, discovery6, gaugeUI6, TWAI7, catalogoRPC4+5baselineendpoint e reviewjournal3, runtimebaseline6+degraded. I test SDK/NVS/hash comportamentali sono **non crittografici** e non provano brownout/wear/radio/tempo fisico. I175Swift comprendono preparazione11, sessioni qualificate9 e regressioni lease2 oltre ai153precedenti.

CrossbuildSDK aggiornati, nessuna esecuzione:

| Progetto | Evidenza | Binario |
|---|---|---:|
| Main skeleton |21core+4shared compilati, linker scarta core non usato |170624byte|
| BLEstudio |9entrypoint trattenute, mainvuoto, JSONRPC aggiornato |449920byte|
| OTAstudio |27entrypoint installer/verifier/adapter/boot, mainvuoto |265088byte|
| Platformstudio |Partizioni/TWAI+runtime/discovery/config/journal e SDK reali, mainvuoto |223312byte|

Prova nativa TF-PSA-Crypto ripetuta: **55vettori/13manifesti accettati**, vereRSA/SHA portable, non providerESP32. Report ora include anche l’header installer transitivo e snapshot vendor dal commit fissato. Configurazioni di sviluppo non dimensionano flash/PSRAM/V1 o2MiBlog.

ProgettoXcode/3risorse rigenerabili, checkK0/diff, hashreport/review pertinenti e scansione file Git-eligible chiusi prima del commit. I report storici rimangono snapshot dei rispettivi sorgenti; BOOT_HEALTH_REVIEW ha un follow-up coordinatore con hashfinali, senza attribuirlo a una seconda review indipendente.

## Lavoro ancora necessario

Proprietario applicativo/main e boardprofile, composizione storage/discovery/runtime/UI/BLE/OTA con boot health reale, reset/provisioning e qualifica nativa/routeOTA, render/display/touch/power/IGN, PID standard oltre53scalar, recovery/performancejournal e transazione archive→inbox. Restano trust/chiavi produzione, condizioni salute quantitative, memorymap finale e prove fisiche/CloudKit dopo membership. I gate non vengono aperti per effetto dei test host. Backlog operativo aggiornato in [SOFTWARE_COMPLETION](../docs/SOFTWARE_COMPLETION.md); riprendere l’integrazione, non riscrivere i moduli già disponibili.
