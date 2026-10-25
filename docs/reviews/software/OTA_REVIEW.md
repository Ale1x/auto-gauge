# Review indipendente OTA v0.1

24 ottobre 2026. Sola lettura di `shared/reference/ota_manifest.py`, `tests/test_ota_manifest.py` e `docs/protocols/OTA_V0.1.md`. Nessuna modifica al verificatore, alle fixture o al contratto.

## Esito

Nessun P0/P1 trovato nel perimetro del verificatore offline. Il rilievo P3 sul controllo della dimensione della chiave è stato corretto dal coordinatore e verificato nuovamente; nessun rilievo aperto in questo perimetro. La firma viene comunque verificata contro una chiave locale fidata: il problema non consente di sostituire il trust anchor tramite il pacchetto.

## P3 RISOLTO — la lunghezza della firma non dimostrava RSA esattamente 3072 bit

Posizione precedente alla correzione: `shared/reference/ota_manifest.py:145–152`. La chiave veniva limitata per dimensione del file e la firma deve avere 384 byte, ma non veniva verificato il numero di bit del modulo della chiave pubblica. Più moduli RSA leggermente inferiori a 3072 bit hanno comunque firma lunga 384 byte. Di conseguenza il verificatore accettava un trust anchor che non rispetta il formato RSA3072 dichiarato dal contratto.

Riprodotto con OpenSSL 4.0.3: generazione effimera `genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:3071`, esportazione pubblica, manifesto valido e firma PSS con SHA256/MGF1-SHA256/salt32. La chiave prodotta nella prova è stata riportata da OpenSSL come **3070 bit**; firma384byte e `verify_package` hanno restituito successo (`release_counter=1`). File e chiavi esistevano soltanto nella directory temporanea, poi rimossa; nessuna chiave salvata nel repository. Il contenuto immagine era sintetico,11byte.

**Correzione verificata:** il coordinatore ora legge la chiave con bound8192byte, ne conserva una sola copia temporanea, richiede esito positivo di `openssl pkey -pubin -text -noout` e header RSA3072/Modulus, poi verifica PSS sulla stessa copia. Questo evita che la qualificazione e la verifica usino letture differenti del file originale. La regressione con chiave inferiore a3072bit viene respinta e una seconda chiave RSA3072 valida ma estranea al trust anchor non autorizza il pacchetto.

Rieseguita la suite dopo il fix: **10/10 test OTA superati** con OpenSSL4.0.3. La dipendenza dal formato testuale dell’output OpenSSL è esplicita e fallisce chiusa: una variante non riconosciuta causa rifiuto, mai salto del controllo; è un limite di portabilità da verificare con l’OpenSSL dell’host CI. Nessun parser ASN.1 o algoritmo di firma implementato nel progetto. Il rilievo rimane storico e non è una vulnerabilità aperta del riferimento attuale.

## Evidenze positive verificate

- Dieci test OTA superati dopo la correzione con firme/verifiche OpenSSL reali e chiavi effimere: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p test_ota_manifest.py -v`.
- Nessuna shell; opzioni PSS, hash, MGF1 e salt esplicite. OpenSSL assente, timeout o esito nonzero falliscono chiusi.
- Input envelope bounded a4096byte, profondità4, JSON senza duplicati anche escaped, schema chiuso, tipi e Base64 canonica. Firma e dominio coprono tutti i campi del manifesto normalizzato.
- Hardware, capacità inattiva e replay floor arrivano da contesto fidato. Counter uint64 canonico, stretto maggiore del floor, terminale senza wrap; bool esclusi dai campi numerici. Il verificatore non avanza stato né modifica il journal.
- Hash streaming con rifiuto di troncamento, append e modifica contenuto. Non viene letto oltre il primo chunk che supera la lunghezza firmata.

## Limiti dichiarati, non nuovi difetti

Il risultato riguarda il file letto durante la verifica, non un file riaperto dopo. La documentazione richiede di verificare i byte effettivamente scritti. Replay floor durevole, trust anchor/rotazione, firma immagine SDK, validazione formato/chip, slot, boot di prova, salute e ordine dei commit antirollback non sono implementati, né vengono dichiarati funzionanti. Questa review non qualifica un aggiornamento sul dispositivo o hardware/eFuse.
