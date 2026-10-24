# OTA — pacchetto firmato e verifica offline v0.1

24 ottobre 2026. Riferimento di sviluppo, **non una release installabile**. Non scrive flash, non seleziona slot, non attiva secure boot/eFuse e non contiene chiavi di rilascio. Non richiede membership Apple.

## Implementazione verificabile

`reference/ota_manifest.py` valida un envelope JSON massimo4096byte, profondità massima4, schema chiuso senza duplicati o numeri floating. Il manifesto contiene esattamente:

| Campo | Vincolo del riferimento |
|---|---|
| schema_version | Intero1, booleani esclusi |
| target | `esp32s3` |
| hardware_id | ID ASCII esplicito, confrontato con il contesto fidato del dispositivo; nessun ID A0 qualificato è ancora fissato |
| firmware_version | Etichetta ASCII1–64byte; mai usata per ordinare le release |
| release_counter | Stringa uint64 canonica, strettamente maggiore del contatore accettato fidato |
| image_bytes | Intero positivo≤16MiB e≤capacità effettiva dello slot inattivo fornita dal chiamante;16MiB è solo un bound del riferimento |
| image_sha256 |64cifre esadecimali minuscole, hash dell’intero file senza byte aggiunti |
| signing_key_id | Seleziona una chiave pubblica nella mappa fidata locale, mai una chiave contenuta nel download |

L’envelope ha esattamente `manifest`, `signature_algorithm` e `signature`. Algoritmo candidato di sviluppo: RSA3072-PSS, SHA256, MGF1-SHA256 e salt32byte; firma384byte in Base64 canonica. I byte firmati sono il prefisso `AUTO-GAUGE-OTA-MANIFEST\0V1\0` seguito da JSON del manifesto ordinato per chiave, senza spazi, ASCII escaped. Firma e hash del contenuto sono controlli distinti. Forme JSON semanticamente equivalenti vengono normalizzate dopo validazione; duplicati anche con chiavi escaped sono respinti.

La verifica usa OpenSSL CLI con argomenti espliciti e timeout, senza shell o algoritmi di firma implementati qui. Controlla anche che la chiave pubblica abbia esattamente3072bit: la sola firma384byte ammetterebbe chiavi3070bit. Legge una copia bounded della trust anchor e usa la stessa copia per controllo tipo/dimensione e verifica. Il riconoscimento fail-closed dell’output testuale `openssl pkey` è coperto dai test; un cambiamento di formato del tool deve bloccare, non saltare il controllo. L’immagine viene letta a chunk64KiB e confrontata per lunghezza/hash. Il risultato `VerifiedPackage` descrive lo snapshot verificato; non promette che un file modificato in seguito sia ancora valido. Non avanza alcun contatore o journal.

## Prove e confine di sicurezza

`tests/test_ota_manifest.py` genera una chiave privata RSA effimera soltanto nella directory temporanea del test. Nessuna chiave privata/credenziale è salvata nel repository. Esegue firme/verifiche reali e rifiuta alterazione di manifesto/firma/immagine, PKCS1v1.5, dominio errato, hardware diverso, capacità insufficiente, replay, overflowuint64, schema/tipi/duplicati/depth e firma malformati. I file binari di test sono dati sintetici, **non immagini ESP32 valide**.

`release_counter` è una precondizione host contro installazioni obsolete, non l’antirollback hardware ESPressif. La sua provenienza fidata e durabilità non sono implementate. Firma del manifesto non sostituisce la firma immagine Secure Bootv2 né la verifica di chip/revision/partizione del bootloader. Nessuna resistenza a riscrittura fisica/rollback del filesystem è dichiarata.

## Integrazione target ancora necessaria

1. Fissare hardwareID, partizioni effettive, firma dell’immagine completa e capacità/margine dopo driver/asset/BLE. Lo skeleton170624byte non dimensiona la V1.
2. Verificare firma del manifesto con trust anchor locale. Trasferire con protocollo bounded su sessione BLE autenticata; aggiornare solo slot inattivo e verificare hash dei **byte effettivamente scritti**, senza assumere immutabile un file già controllato.
3. Eseguire i controlli ESP-IDF immagine/chip/signature e `esp_ota_end` prima della selezione boot. Errore non seleziona l’immagine e non sovrascrive lo slot attivo.
4. Boot di prova con rollback SDK. Non dichiarare valida l’immagine per il solo avvio: qualificare le condizioni di salute, incluse guardie fail-closed e accesso a configurazione/log. Reset/power loss prima della conferma deve consentire fallback secondo il bootloader configurato.
5. Coordinare journal, conferma salute e avanzamento contatori senza distruggere prematuramente il fallback. Serve progettazione e fault injection dell’ordine reale, non un booleano host. Politica di chiavi/rotazione/revoca e recovery USB da verificare prima di provisioning; nessuna eFuse viene programmata dal progetto attuale.
6. Collaudare trasferimento interrotto, immagine tronca/incompatibile, verifica fallita, boot fallito e recupero USB su banco. Nessun update via veicolo prima dei gate hardware.

Fonti primarie: [ESP-IDFv6.1 OTA ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/ota.html), [Secure Bootv2 ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/security/secure-boot-v2.html). Il formato envelope sopra è nostro riferimento applicativo; non è il signature block SDK. Membership Apple e pubblicazione App Store non sono dipendenze per questi test offline.
