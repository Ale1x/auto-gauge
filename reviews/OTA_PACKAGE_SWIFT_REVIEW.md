# APP-11 — review indipendente del verificatore OTA Swift

Data: 24 ottobre 2026. Ambito: sette file del core OTA, quattro file di test/fixture Swift, confronto con `shared/reference/ota_manifest.py` e vettori OpenSSL comuni. Review separata dall'autore; nessuna modifica all'implementazione.

**Esito: nessun difetto concreto trovato nel perimetro esaminato.** Questo esito non qualifica un aggiornamento operativo, la sicurezza del trasporto o il dispositivo fisico.

## Evidenza esaminata

- Il parser impone envelope di 1…4096 byte, oggetti/stringhe/interi, profondità e numero di membri limitati, schema chiuso e consumo completo. I duplicati sono controllati dopo il decoding delle chiavi. I campi accettati sono ASCII e limitati; numeri frazionari, esponenziali, bool, array, overflow e trailing input non producono un manifesto valido.
- Il release counter è decodificato direttamente in UInt64 e confrontato con la sua forma decimale canonica. I byte firmati coincidono con dominio e JSON ordinato/compatto del riferimento Python, inclusi entrambi i NUL del dominio. Il profilo ESP aggiunge esplicitamente versione ≤31 e dimensione multipla di16 al riferimento generico.
- La trust anchor proviene soltanto dalla mappa locale del chiamante. Il DER deve contenere esattamente SEQUENCE e due INTEGER positivi/minimali; lunghezze indefinite/non minimali e trailing data sono respinti. Il modulo è esattamente3072bit, confermato anche dagli attributi Security e dal block size384. Non viene tentato un fallback SPKI/PEM.
- La firma è base64 canonica di384byte. L'algoritmo `.rsaSignatureMessagePSSSHA256` è fisso e supportato dalla chiave. Il `SecKey.h` dell'SDK macOS installato, righe960–962, specifica MGF1-SHA256 e salt32byte. I test reali rifiutano salt20, MGF-SHA384, PKCS1 e dominio errato.
- Envelope, chiave e immagine sono copiati con `Data(bytes:count:)` prima del loro impiego critico. Il digest è calcolato sullo snapshot poi restituito. Il candidato ha sole proprietà pubbliche `let`, nessun initializer pubblico e nessuna conformità Codable: un client esterno al modulo non può fabbricare tale esito. La copia restituita mantiene la semantica di valore di Data. Nessuno stato di release, boot slot o floor viene avanzato.
- Il codice aggiunto usa Foundation, Security e CryptoKit dell'SDK Apple. Nessuna nuova UI, UIKit o dipendenza esterna; target iOS17 invariato. Letti la skill SwiftUI Pro richiesta e i riferimenti Swift, hygiene, API e performance pertinenti alla review parziale del core.

## Verifica eseguita dal reviewer

`swift test --package-path mobile --filter 'OTAManifestTests|OTAPackageVerifierTests'`: **9 test passati, zero failure**. Il generatore ha creato chiavi effimere e46vettori comuni;7pacchetti accettati e39rifiutati tramite Security/CryptoKit reali su macOS. Verificate inoltre ragioni di errore, canonical bytes, UInt64.max e proprietà degli snapshot dopo alterazione di memoria NSMutableData sorgente e di una copia del chiamante.

La build/package usata dai test è quella locale. La review non ripete le build simulator/device già registrate dall'incremento e non attribuisce a questi test esecuzione crittografica su iPhone.

## Limiti da preservare

La memoria sorgente deve restare stabile durante la chiamata: nessuna API Data rende lecite scritture concorrenti attraverso puntatori esterni. Per immagini fino a16MiB la futura UI deve eseguire questa API sincrona fuori dal MainActor. Il candidato verifica soltanto firma, policy locale e digest di uno snapshot: non valida formato bootabile, descriptor/chip, SecureBoot, trasporto BLE, scrittura flash, selezione boot, salute, rollback o floor durevole. Trust anchor, hardwareID, capacità e acceptedCounter devono provenire da policy locale affidabile. Nessuna chiave di prodotto è inclusa.

## Impronte dei file di implementazione esaminati

| File | SHA256 |
|---|---|
| `mobile/Sources/GaugeCore/OTA/OTAManifest.swift` | `cbc9804888940ef95c44763acacd691615fd47c70ab50d009887d9660ee4d531` |
| `mobile/Sources/GaugeCore/OTA/OTAPackageError.swift` | `4978bb8d5287b3201d70fdeb415efc549bd068078eaa738fff942ba5cab8c5de` |
| `mobile/Sources/GaugeCore/OTA/OTAPackageVerifier.swift` | `0a3c7e8bb956783180a0e091529665f8c37ae0d4bcfa1b18762f73d055629b2b` |
| `mobile/Sources/GaugeCore/OTA/OTARSAPublicKey.swift` | `8e41f3e45b37d3fdcee06dff619a1f30ec9ee33370c6f6561bcabd79ffdbc3fa` |
| `mobile/Sources/GaugeCore/OTA/OTAStrictJSON.swift` | `5ac2a42b9c5cc2c726dace5e066530a4e8b6933c5ae106e55c8c07cdc6b73396` |
| `mobile/Sources/GaugeCore/OTA/OTAStrictJSONParser.swift` | `fa18629491c52f4034e72ce61adf233a38b9dd73ec3db04b62f64053107dffb2` |
| `mobile/Sources/GaugeCore/OTA/VerifiedOTACandidate.swift` | `c78e7bb4d712345bb21e0c2c9719257dae0636bb3ba2d892a7eab838a90137e4` |

Priorità: nessuna correzione bloccante individuata in APP-11; mantenere i limiti sopra nei futuri flussi OTA.
