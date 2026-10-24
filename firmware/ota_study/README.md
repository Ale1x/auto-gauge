# OTA-03: installer SDK isolato, non eseguito

24 ottobre 2026. Questo progetto compila e collega un percorso ESP-IDF reale per il solo slot OTA inattivo. **`app_main` è vuoto e non invoca alcuna API.** Le funzioni installer, se chiamate in futuro con callback qualificati, cancellerebbero/scriverebbero flash e potrebbero cambiare il boot. Nessun dispositivo è stato collegato o aggiornato durante queste prove.

## Sequenza e confini

`ag_ota_installer_init` richiede un oggetto statico azzerato, un proprietario esclusivo di tutte le operazioni OTA/partizioni e tre hook fidati: verificatore crittografico del manifesto, autorizzazione locale corrente legata alla lease e clock monotono. Il verificatore target deve controllare firma/trust anchor, hardware, capacità e release floor: **non è implementato da questo studio**, non può essere un booleano del telefono. Il verificatore Python host precedente resta distinto e non viene eseguito sul microcontrollore. Le chiavi di produzione non sono provisionate.

1. `begin`: verifica prima il manifesto, ricontrolla tempo/autorizzazione, richiede running OTA VALID e boot==running, due slot registrati distinti/non sovrapposti e otadata8192byte. Chiama `esp_ota_begin` soltanto sullo slot opposto. Factory/PENDING_VERIFY, altri boot già selezionati e geometrie incompatibili sono rifiutati. Lo studio non effettua il bootstrap iniziale da factory.
2. `write`: chunk1…1024byte in ordine e nell’esatta lunghezza autorizzata, offset non riutilizzati, policy/clock freschi dopo le letture di metadati. SDK write riuscita significa accettazione SDK, non una promessa di durabilità per ciascun chunk. Nessuna ripresa/riprogrammazione automatica dopo errore.
3. `finish`: lunghezza completa, `esp_ota_end` per verificare l’immagine secondo configurazione SDK, poi SHA256 PSA dei byte effettivamente riletti dalla partizione e confronto con hash autorizzato. Verifica anche versione descriptor/floor secure_version. SDK end consuma sempre il handle: il codice non lo abortisce dopo end.
4. `select`: operazione separata, ricontrolla partizioni/autorizzazione e ricalcola l’hash dello slot per rilevare modifiche successive a finish. Usa `esp_ota_set_boot_partition`, senza bypass della verifica SDK. Prima della chiamata marca `selection_may_have_changed`: errore o revoca/timeout dopo la chiamata possono significare selezione già avvenuta. Nessun retry cieco o rollback inventato.

SDK begin può invalidare metadati dello slot **inattivo** in otadata già prima di select; la separazione delle API non significa otadata immutato durante il download. I byte dello slot running sono esclusi dal writer. Le garanzie fisiche del bootloader e di otadata sotto brownout non sono dimostrate da questo studio.

Un rilievo della review del sorgente SDK fissato è stato corretto: `esp_ota_begin` può pubblicare un handle e poi fallire l’erase senza liberarlo. Il codice conserva tale handle anche su errore e tenta abort; fallimento cleanup rimane esplicito. Un’installazione per boot e nessun altro proprietario OTA sono prerequisiti anche per escludere il wrap del contatore handle SDK. Un rifiuto di autorizzazione prima dell’ammissione lascia IDLE, lease e clock non associati: non occupa il ciclo di un futuro chiamante valido. Dopo l’ammissione, fault/cancel non riaprono il ciclo: serve gestione esplicita al nuovo boot, con riconciliazione dello stato reale.

Tutti gli accessi usano lease corrente, clock senza regressione e deadline assoluta/idle, compreso il polling senza nuovi byte. Hook non rientranti, serializzati sullo stesso proprietario; nessun writer concorrente può alterare i byte fra hash e selezione. Limiti H:16MiB immagine,4096byte manifesto, versione ASCII terminata32byte, timeout totale massimo15minuti. Sono limiti di sviluppo, non requisiti V1 congelati.

## Riproduzione senza dispositivo

```sh
sh firmware/ota_study/build.sh
python3 firmware/ota_study/report_build.py
python3 -m unittest discover -s tests -p test_ota_installer.py -v
```

Build SDKv6.1 fissato a `fff9895c82d744c7237be8847347bdd1b07c6643`; tutte7entry point sono trattenute nel link con le vere dipendenze esp_ota/PSA. Report controlla main senza chiamate e hash sorgenti/artefatti. Configurazione separata4MiB con factory+due OTA da1MiB e rollback abilitato; **non** è la mappa della V1 e non dimensiona i2MiB di log. Secure Boot/encryption/antirollback hardware non sono attivati; l’installer rifiuta la compilazione con antirollback hardware in questo studio. Il flag SDK di capacità Wi-Fi può risultare attivo: i simboli init radio sono assenti e il main non avvia radio.

L’harness host compila il C di produzione con shim comportamentali SDK e **hash model non crittografico** esplicito. Copre ordering, errori e byte letti, non la correttezza SHA/SMP/flash del vero SDK. ASan/UBSan e8gruppi includono pre/post allocazione su begin fallito, abort fallito, offset/lunghezza, manifest/auth prima di erase, partizioni/boot invalidi, timeout/regressione/IO lento, lease vecchie, reentry, hash/descriptor e selezione ambigua. Le firme reali del manifesto sono provate nei test host Python separati; la correttezza/integrazione SHA del vero SDK sul target non è stata eseguita qui. Non si attribuiscono al modello le garanzie crittografiche della libreria.

Restano necessari verificatore e trust anchor target, protocollo di trasferimento autenticato, salute del boot di prova, coordinamento del contatore durabile con rollback, recovery USB e prove fisiche di interruzione. Questo codice non conferma salute, riavvia, avanza floor durabili o brucia eFuse. Non è una release di aggiornamento utilizzabile.

Fonti primarie lette: [OTA ESP-IDFv6.1 ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/ota.html) e `components/app_update/esp_ota_ops.c`, `include/esp_ota_ops.h`, `esp_partition.h` del checkout fissato. [Review sorgente](../../reviews/OTA_INSTALLER_SOURCE_REVIEW.md), [harness e review indipendente](../../reviews/OTA_INSTALLER_REVIEW.md).
