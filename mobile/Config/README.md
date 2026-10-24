# Configurazione Apple approvata

Team `ZDQFU74347`, bundle `it.ale1x.autogauge`, container `iCloud.it.ale1x.autogauge`, primo ambiente Development. Sono identificatori approvati in DEC-013, non credenziali.

Il generatore predefinito mantiene firma disabilitata e `AutoGaugeCloudDevelopmentEnabled=NO`; non include entitlement CloudKit e non inizializza CKContainer. App/fixture/simulazione non eseguono upload.

```sh
python3 mobile/scripts/generate_project.py
```

L’opzione esplicita seguente prepara gli entitlement Development e le impostazioni di firma automatica con il team approvato. Non effettua provisioning o invia dati da sola; il provisioning viene verificato separatamente dal coordinatore.

```sh
python3 mobile/scripts/generate_project.py --cloud-development
```

La chiave Info.plist `AutoGaugeCloudDevelopmentEnabled=YES` abilita il factory dell’adapter APP-05 soltanto su dispositivo con profilo incorporato Development coerente. Senza profilo (e sempre sul simulatore) il factory mostra non configurato e non costruisce CKContainer. La sola opzione del generatore non basta. Non viene introdotto un account email separato né un owner unico BLE. Fixture e archivio simulato sono esclusi dalla sync anche in build firmata. Verifica account e sync sono pulsanti distinti; nessun upload automatico.

Il proprietario al momento non ha un abbonamento Apple Developer Program. Apple richiede una membership attiva per [abilitare CloudKit](https://developer.apple.com/documentation/cloudkit/enabling-cloudkit-in-your-app), anche nell’ambiente Development. La verifica del coordinatore sulla copia APP-04 ha inoltre ricevuto `No Account for Team ZDQFU74347` e assenza di profili per il bundle. Nessun container/profilo remoto è stato creato o validato. La base locale/simulata resta utilizzabile senza abbonamento; non viene sottoscritto alcun servizio.

Comandi di provisioning riservati al coordinatore, dopo disponibilità di membership/capability/profilo; non sono stati eseguiti da questo incremento:

```sh
python3 mobile/scripts/generate_project.py --cloud-development
xcodebuild -project mobile/AutoGauge.xcodeproj -scheme AutoGauge -configuration Debug -destination 'generic/platform=iOS' -derivedDataPath mobile/DerivedDataSigned -allowProvisioningUpdates build DEVELOPMENT_TEAM=ZDQFU74347 CODE_SIGNING_ALLOWED=YES
```

Il comando richiede un account Xcode autorizzato per il team e un profilo che includa CloudKit Development. Per tornare alla configurazione salvata sicura:

```sh
python3 mobile/scripts/generate_project.py
```

Schema iniziale creato solo da una futura sync esplicita di dati reali: record `AGHistoryV1` nella private DB, campi schemaVersion(Int64), payload(Bytes), digest(String). [RecordSavePolicy Apple](https://developer.apple.com/documentation/cloudkit/ckmodifyrecordsoperation/savepolicy): il client usa `ifServerRecordUnchanged`; non forza overwrite su conflitti. Non occorrono indici query per fetch per recordID. Nessuna operazione schema/provisioning/upload è inclusa nella generazione o nei test fakeDB. Il progetto salvato resta unsigned/cloud disabilitato.
