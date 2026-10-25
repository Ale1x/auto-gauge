# Migrazione iOS 27 — 25 ottobre 2026

DEC-015 recepisce la richiesta del proprietario: il target minimo diventa iOS27.0.

- Generatore Xcode e progetto rigenerato: deployment target27.0 in Debug e Release, applicabile anche all'opzione cloud-development.
- Package GaugeCore: piattaforma iOS27.0, test host macOS13 invariati. La versione piattaforma stringa evita un cambio non richiesto della modalità linguaggio del package; compilatore locale Swift6.4.
- RootView usa direttamente Tab: rimossi guardia iOS18 e vecchio fallback tabItem per iOS17. Navigazione/stato per scena conservati.
- Decisioni, README corrente e generatore di tracciabilità aggiornati; le verifiche e immagini delle versioni precedenti restano storiche.

## Verifiche concluse

Xcode27.0 (27A266a), AppleSwift6.4, SDK iPhoneOS/Simulator27.0:

- `swift test --package-path mobile`:191test, zero failure.
- Build Debug `generic/platform=iOS Simulator`, unsigned:exit0, nessun warning.
- Build Debug `generic/platform=iOS`, unsigned:exit0, nessun warning.
- Info.plist di entrambi i prodotti: MinimumOSVersion27.0, bundle it.ale1x.autogauge; SDK rispettivamente iphonesimulator27.0/iphoneos27.0.
- `swift package dump-package`: iOS27.0 e macOS13.0.
- Rigenerazione progetto/fixture riproducibile, `check_k0.py` e `git diff --check` passati.

Log locali: `/tmp/ag-ios27-swift.log`, `/tmp/ag-ios27-sim.log`, `/tmp/ag-ios27-device.log`.
Prodotti locali: `/tmp/ag-ios27-sim/Build/Products/Debug-iphonesimulator/AutoGauge.app` e `/tmp/ag-ios27-device/Build/Products/Debug-iphoneos/AutoGauge.app`.

Al termine della sola migrazione, prima della revisione UX, i runtime Simulator installati erano iOS26.4/26.5: nessuna esecuzione su iOS27 è stata dichiarata o tentata su un runtime incompatibile. Nessun download runtime, installazione su iPhone, provisioning, firma iOS, cloud/radio live o pubblicazione eseguiti. Il precedente bundle output/software con delivered_commit22e621c resta una consegna V0.13 con targetiOS17; per produrre una nuova consegna iOS27 usare `python3 scripts/build_software_bundle.py --output /percorso/vuoto`.


## Aggiornamento successivo — revisione UX e runtime installato

Il runtime **iOS 27.0 (24A434)** è stato scaricato e installato. La revisione UX è stata eseguita sul simulatore iPhone 13 Pro Max: 7 prove UI passate, oltre a 196 prove GaugeCore e 8 prove del modello app. Temi automatico/chiaro/scuro, demo completa e salvataggio semplificato sono descritti in [DesignValidation/UX27](DesignValidation/UX27/README.md), con le immagini finali. Le limitazioni e i risultati del precedente passaggio di sola migrazione rimangono una registrazione storica.
