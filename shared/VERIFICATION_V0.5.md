# Verifica del quinto incremento

24 ottobre 2026. Base requisiti v0.2.8, DEC-011/014 preservate. Il proprietario intende valutare l’abbonamento Apple dopo il lavoro eseguibile senza membership. Nessuna iscrizione, provisioning, radio, flash o eFuse eseguiti.

## Verifica integrata eseguita

Il coordinatore ha completato `python3 scripts/check_software.py`: **67testPython,89testSwift e9/9CTest passati**, con ASan/UBSan per il C e build iOS Simulator unsigned riuscita. Rigenerazione del progetto Xcode e delle risorse JSON identica byte per byte. `scripts/check_k0.py` conferma consistenza dei registri/evidenze, non superamento del gate.

Build ESP32-S3 con SDKv6.1 riuscita:14sorgenti firmware e4condivise (parserconfig/frame/stream/RPC), `.bin`170624byte, main inerte e linkerGC. Il report è [device-build-report.json](../firmware/device-build-report.json). Non prova runtime, radio, sizingOTA/PSRAM o stack operativo; parser/encoder/storage hanno frame stack statici significativi documentati.

## App

APP-06 implementa recupero CloudKit paginato con archivio distinto per account e checkpoint nella stessa scrittura atomica dei record. Record conservati daB possono convivere con perdite osservate daA. Cursore scaduto richiede nuova scansione esplicita, che conserva i dati; account change e cancellazione invalidano le operazioni precedenti. Query CK non è snapshot: «scansione terminata» non significa backup completo/atomico né replica delle cancellazioni. [Review indipendente](../reviews/CLOUD_RESTORE_REVIEW.md),13test dedicati.

APP-07 aggiunge pipe/RPC/stream e adapter CoreBluetooth opt-in. Il manager nasce soltanto da azione esplicita; scan10s, write withResponse frammentate sul limite effettivo, CCCD/indicate prima delle richieste, deadline frame10s/request30s H, queue e buffer bounded. Nuova generazione isola callback vecchi. Ultimo ACK ATT perso può significare commit già avvenuto: esito sconosciuto blocca altre apply finché una rilettura valida non risolve. Solo revision_conflict/invalid_configuration espliciti sono rifiuti precommit noti; erroreIO successivo resta ambiguo.

La UI nativa espone esclusivamentehello e non abilita letture private/config/log/apply perché hello.authenticated non prova LESC/MITM/identità. Modalità demo distinta; nessuna scansione è stata eseguita dagli agenti. [Review Bluetooth](../reviews/BLUETOOTH_REVIEW.md).

L’integrazione ha trovato un’incompatibilità nascosta dal pipe finto: il client richiedevaop nelle risposte config, mentre il golden condiviso lo omette. Correzione con op opzionale solo per le forme consentite e correlazione rigorosa alla richiesta pendente; op esplicito deve sempre coincidere. Replay delle10transazioni originali inalterate aggiunto, senza riscrivere la fixture per adattarla al client.

## Firmware e codec C

FW-07 implementa ammissione di telefoni nuovi tramite evento locale/passkey, riconosciuti senza nuovo codice, un solo slot; sorgente entropia obbligatoria, sampling uniforme, lease/timeout/cooldown e hook di sicurezza fidati. Nessuna cache N di prodotto imposta, owner unico, cloud o reset per cambiare telefono. Il binding reale LESC/MITM128bit, gestione bond/GAP e display del codice restano da qualificare: [review](../reviews/ADMISSION_REVIEW.md).

Parser wireconfig e RPC tipizzati: ASCII/escape dichiarati, schema chiuso, nessun heap,4096byte/depth/token bounded, integer/uint64 rigorosi, candidato/output immutati su errore. Il codec non autentica, confronta revisione attiva o committa dati. [Parser](CONFIG_JSON_C.md), [RPC](RPC_C_V0.1.md), [stack target](../firmware/PARSER_BUILD.md).

Stream C:4112byte/chunk512, verifica header/CRC, timeout assoluto anche senza byte, clock fault latched e callback non rientranti. Ogni callback accettata è indipendente: errore sul frameB non annulla effetti del frameA nello stesso chunk. [Contratto](C_STREAM_V0.1.md), [review](../reviews/C_STREAM_REVIEW.md).

FW-08 conserva record e sessioni attraverso riapertura/begin_session, con serial globale, raw64, perdite/overwrite persistiti e ACK non distruttivo;2267tagliIO, bitflip4096byte e6fixture/12pagine comuni. È un modello COW a due banchi, non backendV1: O(capacità) perappend, metà spazio duplicato, fino8190record/2MiB (H27min18s a5record/s), nessuna qualifica endurance/throughput. Banco sporco può lasciareREAD_ONLY e bloccare nuovo logging: autonomia dopo crank non ancora dimostrata. Stack statico annidato oltre11KiB: [modello](../firmware/FLASH_HISTORY_MODEL.md), [lettura separata](../reviews/FLASH_HISTORY_REVIEW.md).

## OTA e condizioni di completamento

Verificatore offline RSA3072-PSS/SHA256 con trust anchor locale, manifesto chiuso/dominio, hardware/capacità/release-counter e hash streaming immagine.10test con chiavi effimere; nessuna privata nel repository. Trovato e corretto controllo chiavi3070bit con firma384byte. [Contratto](OTA_V0.1.md), [review](../reviews/OTA_REVIEW.md). Non verifica il formato immagine ESP32, scrive slot o aggiorna contatori durabili: non è OTA target.

Per V1 restano runtime e binding serverBLE/persistenza efficiente/OTA target, catalogo diagnostico completo, pannelloFPC/rail e protezioni/wake/IGN verificati, prove su banco e poi veicolo. Per CloudKit live servono membership e profilo, poi prove account/rete/quote/query reali. Le build e i test simulati non chiudono queste condizioni. Nessun abbonamento richiesto ora né dichiarazione che «tutto» sia già completato.
