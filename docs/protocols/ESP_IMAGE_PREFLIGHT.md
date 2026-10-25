# OTA-02: preflight immagine ESP32-S3, solo host

24 ottobre 2026. `reference/esp_image.py` aggiunge un controllo strutturale distinto dalla firma del manifesto. Non installa immagini, non seleziona boot e non aggiorna contatori/eFuse.

Il profilo di sviluppo accetta esclusivamente un contenitore applicativo ESP32-S3 con hash SHA256 finale e senza settore di firma Secure Boot. Verifica header24byte, chipID9,1…16 segmenti, extent e allineamento4byte delle lunghezze, overflow indirizzi, descriptor applicazione256byte nel primo segmento, padding/checksum XOR, hash finale e assenza di trailing. Versione/progetto sono stringhe ASCII terminate e bounded. Revisione chip (major*100+minor), capacità slot inattivo e floor secure_version sono vincoli fidati forniti dal chiamante, non ricavati dal candidato.

`verify_candidate` legge al massimo16MiB+1 dal path, conserva una snapshot `bytes` immutabile e verifica firma/hash del manifesto e formato su quella stessa snapshot. Richiede anche corrispondenza versione manifesto/descriptor. Restituisce i byte verificati insieme ai metadati; modificare il file originale dopo la lettura non cambia il candidato. Il vecchio `verify_package` mantiene il contratto limitato al manifesto e non viene promosso a controllo formato. I contatori release applicativo e secure_version SDK rimangono distinti.

```sh
python3 shared/reference/esp_image.py candidate.bin \
  --chip-revision 0 --inactive-capacity 1048576 --minimum-secure-version 0
python3 -m unittest discover -s tests -p test_esp_image.py -v
```

Senza opzioni di autenticazione la CLI controlla solo il formato e lo dichiara nell’output. Per eseguire firma e formato insieme aggiungere **tutte** le opzioni `--envelope manifest-envelope.json --public-key trusted-public.pem --key-id KEY_ID --hardware-id HARDWARE_ID --accepted-counter FLOOR`. Se ne manca una, termina prima di leggere il candidato. API combinata `verify_candidate` e CLI riusano trust anchor RSA3072-PSS e policy del manifesto già esistenti. Nessuna chiave privata di prodotto creata; le firme dei test usano chiavi effimere in directory temporanee.

Prove:12test con immagine sintetica a2segmenti, ogni troncamento e mutazione di singolo byte, extent/header/hash/checksum, semantica descriptor con integrità ricalcolata, firma reale, versione incoerente, CLI firmata e cambio del path durante/dopo verifica. Provati anche i binari reali SDK: studio BLE449040byte, progetto `auto_gauge_ble_compile_study`, versione build `a6ed648-dirty`, e skeleton170624byte/versione0.1.0 accettati dal preflight; bootloader21120byte respinto perché privo del descriptor applicativo. Ciò non qualifica quei main inerti come aggiornamenti utilizzabili. Gli artefatti compilati non sono salvati in Git.

Limiti: non valida mappe di memoria/entrypoint, vincoli SPI/MMU, eFuse/revisioni dei blocchi, firma immagine Secure Boot o avviabilità. Un’immagine con settore di firma viene respinta come formato non supportato, mai ignorata la firma. Nessuna falsa equiparazione fra checksum/hash e autenticità. L’installer target deve verificare firma e **byte effettivamente scritti** con le API SDK, gestire slot/otadata/rollback e dimostrare salute prima di confermare; questo incremento non lo implementa. La lettura bounded è un costo RAM host, non il modello streaming del futuro microcontrollore.

Fonti primarie lette: [formato applicazione ESP-IDFv6.1 ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/app_image_format.html), [OTA e rollback SDK](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/ota.html). Controllati anche `esp_app_format.h`, `esp_app_desc.h`, `esp_image_format.c` e `bootloader_common_loader.c` nel checkout SDK fissato a `fff9895c82d744c7237be8847347bdd1b07c6643`: checksum su boundary16 e max revision0/65535 unset concordano. I vincoli più stretti del nostro profilo sono scelte conservative host, non un’emulazione completa del bootloader.
