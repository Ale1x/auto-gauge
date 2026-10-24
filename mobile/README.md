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
- `App/AutoGaugeApp.swift`: UI SwiftUI, editor con salvataggio locale e trasporto simulato. La bozza valida viene conservata in UserDefaults con chiave dedicata; un upload non valido non modifica la configurazione attiva del simulatore. Non è implementata la persistenza atomica del dispositivo reale.
- `Tests/GaugeCoreTests`: fixture comune e 22 casi condivisi, roundtrip JSON senza campi extra, limiti/geometry, rifiuto di servizi e PID non disponibili, quantizzazione, capability fuori demo, rollback simulato e accesso reale negato.

## Scelte provvisorie e limiti

Il target **iOS 17** è una scelta di sviluppo, non il minimo iOS approvato per il prodotto (O-23). Il layout libero con coordinate normalizzate e slider è una proposta per O-10. Attualmente i rettangoli sono validati entro il riquadro; l’anteprima li ritaglia nel cerchio. Sovrapposizioni, leggibilità, vincoli circolari, font e layout definitivi restano da definire. I titoli dashboard derivano dagli ID e non aggiungono campi al contratto comune.

La luminosità manuale 0–100 è il valore logico di sviluppo; minimo effettivo e curva ALS del pannello non sono definiti. La UI conserva il valore manuale anche in automatico. Gli storici sono esempi senza date assolute inventate: import, resume, deduplica e persistenza delle sessioni reali devono essere implementati.

Account/cloud, associazione esclusiva, autenticazione, antireplay e OTA sono indicati come non disponibili. Nessun falso login, aggiornamento scaricato, account creato, backend selezionato o invio a servizi esterni. L’app non considera una connessione BLE equivalente a proprietà del gauge. Il contratto GATT reale e la sicurezza saranno integrati dopo la relativa definizione; questa base non dichiara risolti O-08/O-13/O-15.

Per completare l’integrazione serviranno: trasporto autenticato con conferma di applicazione/persistenza, discovery delle capability reali, import log e tempo affidabile, aggiornamenti firmati con recovery, provider account e policy cloud. Non sono necessari per usare la simulazione.

Fonti consultate: [Apple NavigationStack](https://developer.apple.com/documentation/swiftui/navigationstack), [Apple CBManagerAuthorization](https://developer.apple.com/documentation/corebluetooth/cbmanagerauthorization). La simulazione non richiede l’autorizzazione Bluetooth.

## Verifica eseguita il 24/10/2026

Con Xcode 27.0 (27A266a), SDK iOS Simulator 27.0: build Debug riuscita per arm64 e x86_64. `swift test --package-path mobile`: 11 test superati, inclusi tutti i 22 casi condivisi e roundtrip della fixture JSON. Generazione del progetto verificata deterministica. Nessun test su hardware reale.

Installazione e avvio via simctl riusciti sul simulatore iPhone 17 Pro, runtime iOS 26.4 (`local.autogauge.demo`). Il processo risulta attivo nel simulatore; non è stata eseguita verifica visiva perché Simulator.app non è disponibile nell’inventario CUA della sessione. Non dichiarare l’interfaccia già verificata sullo schermo né un collaudo UI automatico.
