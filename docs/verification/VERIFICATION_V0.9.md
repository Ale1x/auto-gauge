# Nono incremento: verifica OTA offline e manifesto target

24 ottobre 2026. Requisiti v0.2.8 e target iOS17 invariati. Nessuna installazione, attivazione radio/cloud, membership Apple o chiave di produzione.

## Pacchetto mobile APP-11

Il core Swift verifica envelope chiuso, firma RSA3072-PSS-SHA256 con trust locale DER PKCS#1 e hash CryptoKit dell’immagine. Security.framework usa MGF1-SHA256/salt32; nessun fallback crittografico. Il risultato conserva copie profonde immutabili e non può essere costruito tramite initializer pubblico/Codable. Counter UInt64 canonico, hardware/capacità/floor e profilo ESP (versione≤31, size multiplo16) sono controllati senza avanzare stato. Non verifica bootabilità o installazione e non espone UI updater, download o trasporto. [Contratto e prove](../../mobile/README.md), [review separata](../reviews/software/OTA_PACKAGE_SWIFT_REVIEW.md).

## Manifesto target FW-12

Il C di produzione implementa parser bounded4096byte, canonicalizzazione con entrambi i NUL del dominio, DER minimale RSA3072, import volatile PSA e verifica PSS-SHA256 a salt32. Il lookup delle chiavi è locale; il pacchetto non fornisce trust anchor. Output transazionale solo dopo verifica e destroy riusciti; cleanup incerto impedisce successo. Il wrapper hook è disponibile ma non collegato a un proprietario platform/main, lookup di produzione o servizio OTA BLE/RPC. Il digest firmato non sostituisce l’hash dei byte scritti eseguito dall’installer. [API e limiti](../../firmware/ota_study/MANIFEST_VERIFY.md), [review indipendente](../reviews/software/OTA_MANIFEST_TARGET_REVIEW.md).

Studio ESP-IDF fissato a `fff9895c82d744c7237be8847347bdd1b07c6643`: crossbuild/link reali,9entry point trattenute (7installer+2manifesto), immagine254160byte. Report e hash sorgenti ricontrollati. `app_main` vuoto; nessun flash, selezione boot, eFuse o inizializzazione radio eseguiti. La tabella studio4MiB/factory+2OTA non dimensiona la V1.

## Prove distinte

- Integrata, eseguita prima del handoff e conservata nel log locale `/tmp/auto-gauge-v09-integrated.log`: **89testPython,130Swift,16/16CTest**, sanitizzatori C e build iOSsim unsigned superati. Build genericdevice unsigned riuscita nel filone mobile. La suite completa non viene ripetuta per sole modifiche documentali.
- Fixture comuni:46casi,7pacchetti completi validi e39rifiutati. OpenSSL genera chiavi test effimere eliminate prima del return; il bundle restituisce solo materiale pubblico. La verifica Python reale distingue profilo generico e profilo ESP più stretto. L’immagine sintetica TEST-ONLY non è firmware bootabile.
- Harness manifesto C:4gruppi/55vettori, tutti i prefix truncation,1000input random e fault PSA, output invariato e cleanup. Lo shim PSA è **non crittografico**, come il modello hash dell’harness installer8gruppi; verifica wiring e guasti.
- Prova nativa facoltativa `python3 scripts/check_native_ota_crypto.py`: produzione C linkata alle vere RSA/SHA portable TF-PSA-Crypto fissate a `a2b32072ea898afc1ed5b6caf6931e36028c91d6`. **55vettori/13manifesti accettati**, incluse firme salt/MGF alterate rifiutate. Nessuna funzione crypto sostituita. C/driver sanitizzati, vendor Debug senza sanitizzatori; cinque header forwarding host senza dichiarazioni acceleratore ESP. Report: [native-crypto-report.json](../../firmware/ota_study/native-crypto-report.json).

## Riproducibilità e chiusura

La prova nativa rifiuta modifiche tracked/untracked/submodule SDK/PSA e anche file ignorati nel PSA. Compila un `git archive` del commit PSA fissato, registrando SHA256 dell’archivio e tree Git; symlink/gitlink e path non sicuri sono rifiutati. Ignored SDK esterni al PSA non entrano nella build nativa. La cache SDK non viene modificata e non si scaricano sorgenti. La prova rieseguita dopo la correzione passa55vettori/13accettati; **5nuovi testPython** passano su repository temporanei (cache alterata, pin, submodule, link e mutazioni successive al check). Il totale Python previsto passa89→94 e viene verificato dalla CI host. Review Swift separata: **9testOTA passati**, inclusi46vettori reali, nessun difetto concreto trovato. Questa evidenza resta esecuzione portable host, non del provider/acceleratore ESP32.

Progetto Xcode e3JSON risorse rigenerati identici; checkK0 e diff whitespace superati. Scansione credenziali/chiavi e dimensioni limitata ai file Git-eligible: nessuna chiave privata o credenziale; il PDF storico `evidence/tps62813-q1.pdf` supera5MiB ed era già versionato, nessun nuovo file oltre soglia. Hash sorgenti dei report verificati prima del commit. I risultati del commit/push e della CI sono verificati separatamente alla pubblicazione.

## Confini del prodotto

Restano trasferimento autenticato OTA, provisioning/rotazione/revoca chiavi e policy di produzione, floor durabile atomico con rollback, boot health e recovery USB, sizing task/heap reale, fault fisici e brownout. Runtime fisico/TWAI/BLE/flash, journal efficiente/recupero e CloudKit live restano da qualificare. Il journal resta circa51min a5record/s su2MiB, non7ore. Hardware/pannello/FPC/rail/protezioni/IGN e standby100µA restano aperti. Nessun ordine, messaggio a fornitori o collegamento al veicolo è stato eseguito da questo incremento.
