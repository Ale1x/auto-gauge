# Sviluppo iOS locale

Baseline verificata il 25 ottobre 2026: Xcode 27.0, Swift 6.4, SDK e runtime iOS 27.0. Target minimo dell’app e del package: iOS 27.0. Verificare sempre gli strumenti effettivamente installati.

Tutti i comandi seguenti partono dalla radice del repository.

## Generazione e build

```sh
python3 mobile/scripts/generate_project.py
swift test --package-path mobile
xcodebuild -project mobile/AutoGauge.xcodeproj -scheme AutoGauge -destination 'generic/platform=iOS Simulator' CODE_SIGNING_ALLOWED=NO build
```

Il generatore scopre i sorgenti Swift e l’asset catalog; aggiorna anche JSON demo e catalogo StoreKit. Per le build normali mantiene cloud e acquisti reali disabilitati. Non usare `--cloud-development` per un semplice test locale.

## Simulatori e test UI

```sh
xcrun simctl list devices
xcrun simctl list runtimes
```

Sono stati creati un simulatore di anteprima e uno separato per i test, entrambi iPhone 13 Pro Max su iOS 27. Non assumere che i loro identificatori valgano su un altro Mac. La GUI dei dispositivi della baseline Xcode 27 è `DeviceHub.app` dentro Xcode.

Esempio con l’identificatore del simulatore di verifica, sostituendo `SIMULATOR_UDID` e scegliendo percorsi di risultato nuovi:

```sh
xcodebuild -project mobile/AutoGauge.xcodeproj -scheme AutoGauge \
  -destination 'platform=iOS Simulator,id=SIMULATOR_UDID,arch=arm64' \
  -derivedDataPath /tmp/auto-gauge-ui-derived \
  -resultBundlePath /tmp/auto-gauge-onboarding.xcresult \
  -only-testing:AutoGaugeUITests/ConsumerExperienceTests/testFirstLaunchOnboardingAndReplay \
  -only-testing:AutoGaugeUITests/ConsumerExperienceTests/testOnboardingLargeTextCanFinish \
  CODE_SIGNING_ALLOWED=NO ARCHS=arm64 ONLY_ACTIVE_ARCH=YES test
```

Non sovrapporre build/test sulla stessa DerivedData o sullo stesso simulatore. Non editare sorgenti durante una verifica in corso. Esportare gli allegati con `xcrun xcresulttool export attachments` soltanto quando il bundle è finalizzato. Se il daemon screenshot non può scrivere in Documents, acquisire in `/tmp/` e copiare il file nel percorso di evidenza.

`--uitesting-reset` pulisce soltanto il namespace di test; `--uitesting` lo riusa. Aggiungere `--uitesting-onboarding` per mostrare l’introduzione se non completata. Il percorso normale usa preferenze separate. Per rivedere la guida senza flag: Impostazioni → Rivedi introduzione.

## iPhone e firma

Bundle: `it.ale1x.autogauge`. Il Personal Team usato nell’ultima installazione locale è `2T8797CCAD`; è un dato della postazione, da riconfermare sull’account Xcode disponibile. Il template CloudKit usa l’identificatore approvato `ZDQFU74347`: non sostituirlo globalmente per installare l’app sul telefono.

Passare `DEVELOPMENT_TEAM`, `CODE_SIGN_STYLE=Automatic` e `CODE_SIGNING_ALLOWED=YES` come override della build firmata, con una DerivedData distinta. Prima di installare/avviare individuare il telefono con `xcrun devicectl list devices`. Il telefono deve consentire lo sviluppo; iOS può rifiutare l’avvio a schermo bloccato. Non cancellare dati o profili per aggirare un blocco di avvio.

Per CloudKit leggere [Config/README](../../mobile/Config/README.md) e [configurazione approvata](APPLE_CONFIGURATION.md). Il catalogo StoreKit locale non crea prodotti App Store Connect e non autorizza vendite live.
