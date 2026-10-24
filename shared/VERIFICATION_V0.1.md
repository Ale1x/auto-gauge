# Verifica del primo incremento software

24 ottobre 2026. Verifica integrata eseguita dal coordinatore dopo il lavoro dei subagenti, su macOS con Xcode 27.0 (27A266a), AppleClang 21 e tool host disponibili. Non sono stati usati dispositivo gauge, vettura o backend remoto.

Comando riproducibile dalla radice:

```sh
python3 scripts/check_software.py
```

| Controllo | Risultato osservato | Portata |
|---|---|---|
| Validatore contratto Python | 22 casi superati | Schema di sviluppo, limiti e tipi JSON |
| CMake/Clang | Build completata | Nucleo C11 host con AddressSanitizer e UndefinedBehaviorSanitizer |
| CTest | 1 eseguibile passato, 0 fallimenti | Configurazione, guardie CAN, log, clock, HAL bloccato e fixture; 19 casi comuni rappresentabili in C |
| Swift Package | 11 test passati, 0 fallimenti | Modelli, trasporto simulato, blocco accesso reale, quantizzazione, capability e 22 casi JSON comuni |
| Xcode build | Completata senza firma | Schema AutoGauge, destinazione generica iOS Simulator |
| Python del progetto hardware | 7 test passati | Analizzatore IGN e guardia contro report ERC falsamente vuoti; nessuna prova hardware |

In aggiunta il subagente app ha compilato arm64/x86_64 e installato/avviato il bundle `local.autogauge.demo` sul simulatore iPhone 17 Pro/iOS 26.4. Il processo è risultato attivo; l'interfaccia non è stata ispezionata visivamente perché lo strumento CUA non seleziona Simulator in questa sessione.

## Differenza tra verifica e integrazione reale

I tre casi JSON con booleani/frazioni sono verificati da Python/Swift, non da un parser device: tale parser non esiste ancora. L'header C di test è generato dalle fixture in directory temporanea e non viene usato come byte stream di produzione. Non è stato eseguito un roundtrip BLE tra app e device.

La configurazione firmware è applicata solo in RAM da un singolo task; non esiste commit flash atomico o conferma remota di persistenza. La coda log misura 4056 byte nel build host del subagente; dimensione e layout sul target restano da verificare. Non è il buffer flash da 2 MiB.

ESP-IDF non è stato compilato: toolchain e `idf.py` non erano disponibili. Nessuna immagine device è stata prodotta, flashata o firmata. Il firmware non ha driver CAN/display/BLE attivi. I test delle guardie CAN non verificano il rilevamento quadro o il target standby elettrico.

Account/cloud, pairing esclusivo, antireplay, firmware OTA firmato, import storico reale, recovery e driver hardware rimangono da implementare. Il successo dei controlli software non chiude K0 o i riscontri HR-01…HR-08.
