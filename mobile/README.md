# Auto Gauge per iPhone — prima base di sviluppo

App SwiftUI in italiano, senza dipendenze di terze parti. Modalità simulata sempre visibile. Il progetto include configurazione del dispositivo, editor delle dashboard con anteprima statica, luminosità manuale/automatica, catalogo di esempio, storici dimostrativi, stato firmware e account non configurato. Non contiene una dashboard live sull’iPhone.

## Avvio e verifica

Dalla radice del repository:

```sh
python3 mobile/scripts/generate_project.py
swift test --package-path mobile
xcodebuild -project mobile/AutoGauge.xcodeproj -scheme AutoGauge -sdk iphonesimulator -configuration Debug -derivedDataPath mobile/DerivedData build CODE_SIGNING_ALLOWED=NO
```

Aprire `mobile/AutoGauge.xcodeproj`, scegliere lo schema condiviso `AutoGauge` e un simulatore iPhone. Il progetto è generato dallo script Python con sola libreria standard; non serve XcodeGen. La firma è disabilitata per questa base: l’esecuzione su un iPhone fisico richiederà configurazione del team e della firma. Non è una build di distribuzione.

`Package.swift` espone `GaugeCore` e i suoi test su host. I test leggono direttamente `shared/fixtures/`, evitando copie divergenti. Il progetto Xcode usa questo package locale.

## Implementazione

- `Sources/GaugeCore/Models.swift`: configurazione Codable conforme a `shared/PROTOCOL_V0.1.md`; chiavi snake_case, ID interi, Service 01 implicito, geometria JSON intera in unità 1/10000. I metadati del catalogo non fanno parte della configurazione wire.
- `ConfigurationValidator`: limiti 1–3 dashboard e 1–5 widget; unicità ID per ambito, geometrie e quantizzazione, versioni/revisioni, luminosità e capability PID. L’autorità sul supporto del PID è il catalogo passato alla validazione, non il catalogo dimostrativo. Il catalogo demo ha solo cinque misure e non prova supporto di alcuna vettura.
- `Sources/GaugeCore/Transport.swift`: interfaccia asincrona per l’integrazione futura; implementazione locale simulata e implementazione non configurata che rifiuta le scritture. Nessuna istanza CoreBluetooth viene creata e non avviene ricerca Bluetooth o trasmissione di dati.
- `App/AutoGaugeApp.swift`: UI SwiftUI con sessione di modifica locale. La bozza valida viene conservata in Application Support con sostituzione atomica del file; la prima apertura può migrare la bozza UserDefaults della base iniziale senza eliminarla. Un errore di salvataggio o ricarica conserva gli snapshot correnti. Non è implementata la persistenza atomica del dispositivo reale.
- `Sources/GaugeCore/Editing.swift`: bozza di lavoro separata dallo snapshot salvato, annulla/ripristino/ricarica, editing su griglia intera 0–10000 e ID stabili dopo rimozioni. I riquadri che escono dal cerchio producono un avviso visibile: la validazione condivisa resta rettangolare.
- `Sources/GaugeCore/Framing.swift`: codec di packet completi conforme a `shared/FRAMING_V0.1.md`, CRC32 IEEE, limite payload4096. Non implementa reassembly/timer/MTU, Bluetooth, autenticazione o operazioni sul dispositivo.
- `Tests/GaugeCoreTests`: fixture comune e 22 casi condivisi, roundtrip JSON senza campi extra, limiti/geometry, rifiuto di servizi e PID non disponibili, quantizzazione, capability fuori demo, rollback simulato e accesso reale negato.

## Scelte provvisorie e limiti

Il target **iOS 17** è una scelta di sviluppo, non il minimo iOS approvato per il prodotto (O-23). Il layout libero con coordinate normalizzate e slider è una proposta per O-10. I rettangoli sono validati entro il riquadro; l’anteprima li ritaglia nel cerchio e segnala visibilmente i riquadri tagliati. Sovrapposizioni, leggibilità, vincoli circolari, font e layout definitivi restano da definire. I titoli dashboard derivano dagli ID e non aggiungono campi al contratto comune.

La luminosità manuale 0–100 è il valore logico di sviluppo; minimo effettivo e curva ALS del pannello non sono definiti. La UI conserva il valore manuale anche in automatico. Gli storici provengono da fixture locali e attraversano il vero importer locale con deduplica, resume e salvataggio su questo iPhone. La sorgente resta dimostrativa: non è implementata la sincronizzazione dal gauge reale. Non vengono inventate date assolute.

DEC-011 consente più telefoni autorizzabili e utilizzabili in momenti diversi, con un solo collegamento Bluetooth attivo alla volta. Non esiste un owner unico persistente e cambiare o aggiungere telefono non richiede reset. Autenticazione e autorizzazione della sessione sono requisiti distinti da integrare. Account/cloud, Bluetooth reale e OTA restano non disponibili: nessun falso login, firmware scaricato, account creato, backend scelto o invio esterno. Nessun protocollo di sicurezza reale è dichiarato implementato.

Per completare l’integrazione serviranno: trasporto autenticato con conferma di applicazione/persistenza, discovery delle capability reali, sync log dal gauge e ancore temporali affidabili, aggiornamenti firmati con recovery, provider account e policy cloud. Non sono necessari per usare la simulazione.

Fonti consultate: [Apple NavigationStack](https://developer.apple.com/documentation/swiftui/navigationstack), [Apple CBManagerAuthorization](https://developer.apple.com/documentation/corebluetooth/cbmanagerauthorization). La simulazione non richiede l’autorizzazione Bluetooth.

## Verifica eseguita il 24/10/2026

Con Xcode 27.0 (27A266a), SDK iOS Simulator 27.0: build Debug riuscita per arm64 e x86_64. `swift test --package-path mobile`: 11 test superati, inclusi tutti i 22 casi condivisi e roundtrip della fixture JSON. Generazione del progetto verificata deterministica. Nessun test su hardware reale.

Installazione e avvio via simctl riusciti sul simulatore iPhone 17 Pro, runtime iOS 26.4 (`local.autogauge.demo`). Il processo risulta attivo nel simulatore; non è stata eseguita verifica visiva perché Simulator.app non è disponibile nell’inventario CUA della sessione. Non dichiarare l’interfaccia già verificata sullo schermo né un collaudo UI automatico.

## Incremento APP-02 / codec SH-02

Selezione dalla lista o dall’anteprima, trascinamento e controlli di posizione/dimensione operano sugli stessi ID e unità intere del contratto. Gli slider vengono limitati in base alla geometria corrente; gli stepper consentono un’unità alla volta e hanno etichette italiane per VoiceOver. Le rimozioni non rinumerano le misure superstiti. L’avviso del bordo circolare è informativo e non impone un nuovo vincolo su O-10.

Le azioni **Salva bozza locale**, **Annulla modifiche**, **Ricarica bozza salvata** e **Ripristina esempio nella bozza** sono distinte. Ripristina modifica solo la bozza di lavoro; Annulla torna all’ultimo salvataggio. Il salvataggio include tutte le dashboard e la luminosità, incrementa la revisione solo dopo scrittura riuscita e non invia dati al gauge. Se il file locale è corrotto al lancio, l’app mostra un avviso e carica l’esempio senza sovrascrivere il file corrotto.

La sostituzione atomica del file è una garanzia del percorso di scrittura locale; non è una prova di resistenza a ogni perdita di alimentazione e non riguarda la flash del gauge. Nessun test UI automatico o verifica visiva aggiunto in questa iterazione.

Il codec framing confronta i15 vettori golden comuni (validi e non validi), verifica il riferimento CRC32 `123456789`, i limiti payload, tutte le troncature di un packet campione e la corruzione di ogni byte del suo payload. Il CRC non autentica, non cifra e non impedisce replay; il trasporto reale resta bloccato.

Verifica APP-02/SH-02 del 24/10/2026: **23 test Swift superati**, comprendenti 22 casi configurazione e 15 vettori framing condivisi; build Debug iOS Simulator riuscita per arm64 e x86_64. Nessun nuovo avvio del simulatore né collaudo visivo dichiarato per questo incremento. Resta valido il limite di assenza di integrazione BLE/cloud/OTA reale.

## Incremento APP-03 — storico locale importato

`LogModels.swift` implementa pagine/record/intervalli Codable rigorosi secondo `shared/LOG_SYNC_V0.1.md`: sessioni128bit esadecimali non nulle, sequenze e tempi unsigned64 come stringhe decimali canoniche, PID/ECU interi, stati definiti e raw esadecimale coerente con lo stato. Nessun passaggio attraverso Double per sequenza o tempo; chiavi duplicate/sconosciute, null impropri, frazioni/esponenti per campi interi e dati fuori limite sono rifiutati. Una pagina input è limitata a4096byte,16record e16intervalli.

`LogArchive.swift` contiene un actor che serializza importazioni di **un solo archivio per installazione**. Record, perdite e cursori vengono preparati in uno snapshot candidato, validati e scritti con sostituzione atomica del file; lo snapshot attivo e l’ACK cambiano solo dopo write riuscita. Duplicati identici sono idempotenti; conflitti, gap non giustificati, perdite che contraddicono record importati e record tardivi su una perdita già riconosciuta rifiutano l’intera pagina. Un errore di ricarica conserva lo snapshot in memoria e blocca nuovi import fino a ricarica valida. Il file corrotto non viene sovrascritto automaticamente.

Limiti **di questo prototipo**, non policy retention del prodotto:512KiB di file,1024record complessivi,16sessioni e128intervalli di perdita coalescenti. A capacità piena non si eliminano dati: la nuova pagina viene rifiutata senza ACK nuovo. Nessuna eviction silenziosa e nessuna scelta di retention cloud. La lettura del file è limitata prima della decodifica. È richiesta una sola istanza dell’actor per file; non è implementato il coordinamento tra processi o più istanze che scrivano lo stesso percorso. La sostituzione atomica non dimostra durabilità a ogni power loss né protezione/backup iOS definitiva.

`generate_project.py` richiama `generate_demo_logs.py`, che deriva il bundle `Resources/log-demo-pages.json` dalle fixture comuni: pagina con perdita dichiarata, seconda sessione e pagina timeout (quest’ultima assegnata esplicitamente a una terza sessione dimostrativa). `DemoLogImport.load()` carica le pagine dal bundle; l’app le importa realmente nel file locale al primo archivio vuoto. La schermata Storici mostra record, stati, perdite e cursore locale; il dettaglio usa soltanto tempi relativi all’avvio e bytes raw per i campioni validi. Gli stati non disponibili mostrano `—`, senza vecchi valori. Reimportare le fixture verifica la deduplica. Nessun orario assoluto fittizio né dashboard live.

Ogni installazione/telefono ha il proprio cursore device/session. L’ACK locale non elimina il buffer del gauge per altri telefoni, non attribuisce proprietà esclusiva e non richiede reset al cambio client. Il reset USB con conferma fisica è manutenzione distinta: riguarda credenziali, configurazione e log locali gauge; non elimina cloud o storici già sincronizzati. Questa app non esegue reset reali.

I test importano tutti gli8 scenari comuni da `shared/fixtures/log.scenarios.json`, riaprendo l’archivio dopo ogni operazione. Coprono UInt64.max e sequenze oltre2^53 senza perdita di precisione, conflitti/rollback, store fallito, file corrotto/oversize, sessioni e telefoni indipendenti, limiti senza eviction, primitive invalide e importer delle risorse demo. La UI resta da ispezionare visivamente; nessuna verifica radio/hardware/cloud.

Verifica APP-03 del24/10/2026: **32 test Swift superati**, inclusi gli8 scenari log condivisi con reopen a ogni operazione e le risorse demo realmente importate. Build Debug iOS Simulator arm64/x86_64 riuscita; bundle risorse presente nell’app compilata. Nessun nuovo launch/controllo visivo, nessun collegamento radio o backend.
