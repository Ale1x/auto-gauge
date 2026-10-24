# Audit di completamento — Auto Gauge v0.2.6

Snapshot del 24 ottobre 2026, 14:10 Europe/Rome. Review indipendente, sola lettura di sorgenti/documentazione e scrittura di questo solo rapporto. Base: `SPEC_V0.2.md` integrata da `DECISIONS.md` fino a DEC-011. La SPEC originale non va interpretata ignorando gli addendum. Nessuna suite viene rieseguita in questo audit: i risultati sotto sono quelli documentati nei report di verifica, distinti dall'ispezione diretta dei sorgenti.

**Esito: incremento locale concreto e verificabile; prodotto V1 non completo.** La creazione/pubblicazione di un repository salva e condivide questo stato, non cambia l'esito. Il criterio «completato» non può essere soddisfatto moltiplicando fixture o chiamando produzione uno stub. Gli agenti possono ancora implementare molto software senza fermarsi, ma non possono inventare un pannello corretto, IGN affidabile, misure di corrente, credenziali o approvazioni di servizio.

## A. Implementato e verificato localmente

| Area | Evidenza effettiva | Limite della prova |
|---|---|---|
| App iOS SwiftUI italiana | `mobile/AutoGauge.xcodeproj`, editor 3×5, coordinate intere, bozze locali, brightness, import storico con deduplica/resume/perdite e commit prima di ACK; `mobile/Tests/GaugeCoreTests/` | È software eseguibile locale con fixture. `mobile/App/AutoGaugeApp.swift` crea modello/catalogo demo e salva sul telefono; nessun gauge reale viene modificato. Nessuna dashboard live iPhone introdotta. |
| Protocollo fra implementazioni | Codec C/Python/Swift,15 vettori framing; config/casi JSON e fixture paginazione; `shared/FRAMING_V0.1.md`, `RPC_V0.1.md`, `LOG_SYNC_V0.1.md` | CRC controlla corruzione, non autenticazione/antireplay. Simulatori process-local, non GATT. |
| Persistenza locale telefono | `mobile/Sources/GaugeCore/LogArchive.swift` e test: archivio bounded, atomic replacement, cursori solo dopo write, rifiuto conflitti/gap; riferimento SQLite Python | Durabilità in ogni power loss, iOS data protection/backup e retention di prodotto non dimostrate. Limiti demo512KiB/1024record/16sessioni non sono policy cloud né limiteN di telefoni. |
| Core diagnostico | `firmware/src/{can_policy,obd_decode,isotp_rx,scheduler}.c`: allowlist Service01, capability perECU, stati unavailable/stale/timeout, guardie fail-closed a prepare/dispatch, una transazione | Sette PID implementati:04/05/0C/0D/0F/10/11. ISO-TP RX bounded senza Flow Control TX, nessun driver CAN. Non soddisfa ancora tutti i PID standard Service01 del veicolo. |
| Multi-client e reset | `firmware/src/connection.c`, test e `shared/CONNECTION_POLICY_V0.1.md`: A→B→A senza reset, uno slot attivo, lease contro callback tardivi, journal/reset simulato con interruzioni | Il verificatore reale non esiste. La macchina a stati è seriale, non prova concorrenza radio, erase flash o sicurezza crittografica. Non reintrodurre owner unico; DEC-011 è già deciso. |
| Storico produttore C | `firmware/src/history.c`, fixture CLI e `tools/check_history_interop.py`: ring RAM bounded, pagine≤4096byte, sequenzeuint64, ACK non distruttivi per client | Non è il ring persistente2MiB. `app_main` non avvia il produttore. Sessioni storiche su flash e recupero al power loss ancora assenti. |
| Schema e studi HW | A0 modificabile in KiCad,8fogli,97simboli,336pin registrati,273connessioni esportate verificate; review53connessioni critiche; studi USB/sensing separati con calcoli/netlist/ERC | A0 ha36errori e2warning aperti. Nessun PCB. ERC0 dei blocchi separati è soltanto connettività; nessuno è un'alimentazione USB o un sensing qualificato completo. |

L'ultimo rapporto integrato stabile letto, `shared/VERIFICATION_V0.3.md`, documenta30testPython,32testSwift,3suiteCTest e build simulatore riusciti con sanitizzatori C. **Non sono i conteggi definitivi dell'albero in modifica:** il CMake corrente include anche `gauge_history_tests` e `gauge_history_interop`; gli agenti stanno completando nuovi controlli. Non attribuire a questo audit esecuzioni non effettuate.

### Lavori in corso al momento della lettura

L'agente app sta applicando SwiftUI Pro e verificando il nuovo incremento: non valuto come difetto definitivo l'organizzazione corrente del fileUI né anticipo l'esito della review. Ho letto la skill e il riferimento data flow per limitare questa lettura ai confini demo/reale; non è una seconda reviewUI completa.

L'agente firmware ha comunicato installazione di **ESP-IDF v6.1** dal tag ufficiale, commit `fff9895c82d744c7237be8847347bdd1b07c6643`, cache fuori dal repository. BuildESP32-S3 in corso, esito non ancora acquisito in questo snapshot. La precedente frase README «SDK non disponibile» è storica e non una dipendenza permanente. Una build target riuscita proverà compilazione/link/size del percorso incluso, non avvio su scheda o funzionamento HAL.

## B. Lavoro software ancora eseguibile senza nuove decisioni prodotto

1. **Passare dal core a un runtime target testabile.** Completare build riproducibile ESP-IDF, fissare release/toolchain di sviluppo e registrare map/size/stack; predisporre task/lock, errori, watchdog e adapter di simulazione separati da quelli hardware. Compilare tutti i moduli/codec che il runtime userà: la build di un `app_main` inerte non li esercita automaticamente. Nessun flash/eFuse necessario.
2. **Completare diagnostica Service01 su simulatori.** Ampliare decoder/catalogo con fonti e fixture per PID standard, discovery e sourceECU, indirizzamenti richiesti ISO15765-4, Flow Control con la stessa guardiaTX, scadenze/retry/bus-off. Misurare carico/latency virtuali1/5/15PID e mantenere separato l'obiettivo5Hz dal risultatoECU reale. Non serve domandare di nuovo lo scopeService01, già approvato.
3. **Realizzare pipeline persistente e verifiche d'interruzione.** ParserJSON bounded C per wireconfig, commit/revisione, storageadapter sufile/flash simulata, journal/config/log con fault injection e sessioni attraversoboot. Proporre tabella partizioni con immagini realmente dimensionate, due slotOTA e2MiBlog DEC-006; rilevare conflitti anziché ridurreOTA. Driverflash reale e prove fisiche verranno dopo. ACK diuntelefono non deve eliminare dati deglialtri.
4. **Progettare e implementare il trasporto/sicurezza tecnico di sviluppo.** Specificare UUID/versione/MTU/reassembly/timeout/errori, trust boundary e protezione dei comandi; testare malformed/replay/riconnessione/reset/lease e perdita alimentazione su emulatori. Usare primitive standard delle piattaforme, non CRC o lease come sicurezza. Si può scrivere la proposta e gran parte degli adapter senza backend scelto; l'ammissione di un nuovo telefono e il legame eventuale con account richiedono policy prima dell'attivazione reale. Nessuna eFuse necessaria per sviluppare test di verifica firma/manifest/rollback simulato.
5. **Collegare l'app alla pipeline di protocollo completa in simulazione.** Attualmente editor salva una bozza locale; collegare capability/config-revision/received/applied/persisted ed errori al transport, senza chiamare il salvataggio locale «applicato sul gauge». Proseguire UI/accessibilità/navigazione/layout circolare e test automatici; documentare ipotesi O-10/O-12/O-23 senza congelarle. Aggiungere interfacce account/sync e testoffline/conflitti/cancellazione con servizi locali esplicitamente mock.

Queste attività sono incrementi finiti utili; nessuna richiede di scegliere un cloud arbitrariamente o di allentare le guardie CAN. L'assenza dei servizi reali non giustifica arrestare tutto lo sviluppo.

## C. Tre blocchi P0 concreti per dichiarare V1 completa

### P0-1 — Non esiste ancora un assieme elettrico costruibile e verificato

`hardware/05_AMOLED_TOUCH.kicad_sch` non assegna i 31 pin FPC: il pannello non ha identità, datasheet, rail e sequenze coerenti e confermati. `PANEL_SOURCING_K0.md` registra incongruenze fra codici/cartigli e alimentazioni; USD 10 e «campioni a stock» della risposta Huaxin non chiudono il problema. Senza quei dati non si possono definire driver, alimentazioni, potenza o fit.

`hardware/02_AUTOMOTIVE_POWER.kicad_sch` conserva TVS, FET, SOA, limiti e passivi aperti. Il latch-off in SLEEP può togliere VSUP/INH al percorso di wake; il solo RC non risolve. Il foglio 07 lascia VBUS raw scollegato dal mux finché OVP, corrente, inrush, CC e backfeed non sono chiusi. Lo studio del preregolatore H3,792V conserva dropout, soft start, fault FB e sourcing aperti. Lo studio `hardware/presence_sensing/HW04_REVIEW.md` conserva massimi di leakage non documentati: 0,839 µA è il massimo delle sole resistenze OBD, non del circuito. Nessuno di questi OPEN si chiude con PWR_FLAG o un ponticello.

**Chiusura misurabile:** pannello esatto e disegni/MPN confermati; schema dimensionato, pinout e footprint completi, review indipendente; inviluppo elettrico e termico definito; prove di banco USB-only/OBD-only/both, backfeed, inrush, short, UV/OV, crank e recovery, poi misure IGN/standby/wake. Deve risultare ≤100 µA all'ingresso OBD a 12 V entro 300 s dal quadro OFF nelle condizioni approvate, incluso CAN attivo a quadro OFF. Presenza OBD e attività CAN non dimostrano IGN. Se il metodo senza dati OEM non distingue gli stati sulla Polo, serve una decisione sul compromesso **dopo averne dimostrato il limite**.

### P0-2 — Funzioni operative e sicurezza device assenti

Evidenza diretta: `firmware/idf/main/app_main.c` legge lo stato HAL e resta in loop; `hal_unimplemented.c` ritorna NOT_IMPLEMENTED o BLOCKED per power, display, log, BLE e OTA; il verificatore ritorna false. Non esistono ancora driver attivi per TWAI/transceiver/display/touch/ALS, ring flash da 2 MiB o commit della configurazione sul device. `ag_obd_pid_implemented` ammette solo sette PID. Il gauge non può ancora operare autonomamente secondo PR-003/006/010/014/018.

`mobile/Sources/GaugeCore/Transport.swift` nega il trasporto reale; nessuna istanza CoreBluetooth esegue discovery/pairing. `FirmwareView` disabilita l'aggiornamento. Non esistono sessione autenticata e antireplay reali o una catena OTA firmata con rollback e recovery. Lease e CRC su host non sostituiscono questi requisiti.

**Chiusura misurabile:** due telefoni fisici A→B→A con una connessione attiva, senza reset o cancellazioni; mutazioni non autorizzate e replay rifiutati; configurazione persistente e log ripresi dopo power loss; dashboard, tap, ALS e discovery/decoder reali funzionanti senza iPhone. OTA firmata e compatibile con hardware, avanzamento, interruzioni in più fasi, fallback e recovery USB con policy delle chiavi documentata. Le prove CAN/5Hz richiedono simulatori con più ECU e infine Polo, dopo chiusura della sicurezza hardware; le fixture iniettate non bastano.

### P0-3 — Account e cloud richiesti non sono implementati

`AccountView` disabilita Accedi/Crea account e dichiara il provider non scelto; il codice non contiene backend, credenziali, gestione token, sync HTTPS o ciclo di vita account. Lo storico atomico locale è reale, ma importa soltanto fixture locali. PR-021 resta insoddisfatto anche se editor e importer superano tutti i test.

**Chiusura misurabile:** provider e architettura approvati, ambiente configurato, login/account/signout/revoca e cancellazione verificati, token protetti, sync reale con retry, offline, conflitti, deduplica, retention e trattamento dei dati personali esplicito. Dimostrare che il gauge resti autonomo senza account/cloud. La policy cloud non deve reintrodurre owner unico o reset per cambiare telefono.

## Dipendenze esterne e decisione davvero necessaria ora

**La prossima decisione prodotto utile per rimuovere lo stub account/cloud è O-13:** approvare una proposta concreta di provider/metodi di accesso e regole dati: quali log/configurazioni vadano nel cloud, retention/cancellazione, uso offline e relazione fra account e client autorizzati. Prima della domanda il coordinatore può preparare alternative e trade-off concreti; l'audit non sceglie un servizio, non crea account e non inventa autorizzazioni o spesa.

Va anche specificato **come una persona autorizza un nuovo telefono**: DEC-011 consente N client in sequenza, ma non autorizza chiunque si trovi vicino a modificare il gauge. La scelta crittografica e la proposta tecnica possono proseguire autonomamente; l'esperienza di ammissione e l'eventuale requisito account devono essere coerenti con la policy approvata. Non chiedere di nuovo esclusività o reset: sono già decisi.

Le altre decisioni non vanno trasformate in domande bloccanti immediate per tutto il software. Diametro finale e tolleranze dopo quote complete, inviluppi ambientali e profili per le protezioni, target wake, regole editor/brightness, minimo iOS, distribuzione e chiavi firmware, perimetro prototipo/vendita vanno concordati prima dei rispettivi freeze. Il metodo IGN richiede evidenza: non scegliere un compromesso senza verificarne la necessità.

I contatti già autorizzati risultano inviati in `SUPPLIER_CONTACT_LOG.md`; Huaxin ha risposto, mentre chiarimenti e quotazioni successivi risultano in attesa nel registro letto. L'audit non ha controllato nuovamente Gmail e non dichiara «nessuna nuova risposta» all'istante corrente. Campioni, strumentazione, misura sulla Polo, RF con magnete, cavo 2 m/crimp/protezioni e validazione meccanica non sono sostituibili dal codice. Costo 40–70 EUR per unità, cinque unità e assemblaggio completo con display/cavo sono obiettivi senza offerta confermante: verificarli con sourcing e preventivo, non con la BOM illustrativa.

## Cosa significa «completo» in modo verificabile

- **Incremento software locale completo:** implementazioni e contratti concordi, controlli integrati aggiornati passanti, build target e iOS registrate, limiti e artefatti riproducibili; nessuna funzione mock descritta come reale. È una milestone raggiungibile in questa sessione, con lavori già in corso.
- **V1 completa:** tutte le funzioni PR correnti, incluse account/cloud, BLE, OTA e autonomia, e criteri T-001…T-016 applicabili superati con evidenza sugli assiemi; target 100 µA/300 s dimostrato, limiti 5Hz/wake e ambientali definiti, integrità al power loss e sicurezza testate. K0 e readiness produttiva richiedono le voci hardware/sourcing precedenti, non un numero di test host.

Questo rapporto fotografa una review documentale e di sorgenti, non una certificazione o una review completa di sicurezza, elettrica o UI. Un successivo rapporto build o una nuova risposta del fornitore può chiudere una voce circoscritta, non retroattivamente tutte le altre.
