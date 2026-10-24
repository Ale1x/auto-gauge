# Settimo incremento: storici, grafici e preflight OTA

24 ottobre 2026. Requisiti v0.2.8 invariati. Nessuna radio reale, flash fisica, eFuse, membership o provisioning attivati.

## Risultati verificabili

**FW-10:** nuovo journal append-only a settori, separato dal precedente modello COW. Un append ordinario programma136byte (prenotazioni duplicate e record), senza ricopiare tutti i dati. Footer di eviction committato fuori dal settore vittima, witness e bitmap impediscono di confondere dati confermati poi cancellati con spazio mai usato. Apertura e query degradate non diventano pagine complete; ACK non consuma dati e ogni nuovo boot richiede un SID fresco. L’adapter tipizzato `ag_journal_endpoint_read` collega il contratto history_read dell’endpoint senza inizializzare o cancellare il backend. Non è automaticamente attivo nel CLI o in un runtime fisico.

Prove:8gruppi journal,2573tagli I/O principali più tagli bootstrap,8192posizioni di byte con un bit invertito; regressioni sui dati già confermati cancellati, intent e contatori terminali. Oracolo indipendente480operazioni e6fixture/12pagine golden conservati in test riproducibili. [Formato e limiti](../firmware/JOURNAL_HISTORY.md), [review indipendente](../reviews/JOURNAL_HISTORY_REVIEW.md).

**APP-09:** grafici Swift Charts e tabella dai record effettivamente importati, con filtro sessione/PID/ECU e provenienza separata fra gauge simulato e copia iCloud locale. Sette formule scalari qualificate; errori, timeout, raw incoerenti e formule assenti non diventano zeri. Nessuna interpolazione attraverso interruzioni. Grafico massimo240punti originali, con estremi e continuità conservati quando possibile; troppe interruzioni comportano soli punti. Archivio massimo1024record e tabella50righe per pagina. Tempi UInt64 esatti, sottrazione dell’origine prima della conversione grafica, nessuna data di guida inventata.12nuovi test core, screenshot reali del simulatore; nessun contatto cloud o BLE per questa vista.

**OTA-02:** preflight read-only di contenitore applicativo ESP32-S3, checksum/hash, descriptor/versione, revisione e capacità; API/CLI combinata verifica manifesto RSA e formato sugli stessi byte immutabili. Dodici test con firme effimere, file modificato durante/dopo verifica e rifiuto di immagini/trailing non supportati. [Contratto](ESP_IMAGE_PREFLIGHT.md), [review indipendente](../reviews/ESP_IMAGE_REVIEW.md). Non verifica Secure Boot, avviabilità o byte scritti su flash, non installa e non avanza contatori.

**BLE:** corretti due guasti SDK nello studio NimBLE: CONNECT rifiutato deve restare tracciato fino alla chiusura reale, e gli errori advertising devono produrre fault espliciti. Nessun accesso privato viene concesso durante questi guasti.12gruppi transport più5bridge passano con ASan/UBSan; compilazione/link SDK separati riusciti, immagine449088byte e main inerte. [Review](../reviews/NIMBLE_STUDY_REVIEW.md).

## Verifica integrata

`python3 scripts/check_software.py`: **83 test Python,117 Swift,14/14 CTest superati**, con ASan/UBSan per C e build iOS Simulator unsigned riuscita. Dopo la correzione visiva del banner, ripetute le build iOSsim e genericdevice unsigned: entrambe riuscite, core invariato. Screenshot finali light/dark/testo accessibile ispezionati; titolo e back nativo visibili, tap/gesti e VoiceOver interattivo non verificati. [Review app](../reviews/APP09_REVIEW.md), [anteprime e riproduzione](../mobile/DesignValidation/APP09/README.md).

Build SDK ESP32-S3:17sorgenti core e4condivise; journal e adapter compilati ma non invocati. Lo studio BLE conserva tutte9entry point pubbliche nel link, senza chiamarle. Frame stack statici del journal misurati con la riga compiler SDK e `-fstack-usage`; non sono high-water runtime. Progetto Xcode e tre risorse JSON rigenerati identici; checkK0/diff coerenti. Le build non sono prove runtime.

## Limiti che restano aperti

Il nuovo formato contiene al massimo15360entry, comprese quelle di controllo: circa **51minuti a5campioni/s su2MiB**, prima dei controlli di sessione. Le sette ore erano aritmetica teorica di un formato più piccolo, non una durata dimostrata. Il journal migliora costo di scrittura e capacità rispetto al COW, ma non è ancora backend V1 qualificato: query e controllo SID scandiscono la capacità, una sync completa può richiedere lavoro quadratico; record/erase ambigui restano READ_ONLY senza riparazione automatica. Stack, flash fisica, endurance e convivenza radio vanno misurati. I2MiB restano subordinati al sizing OTA.

Il prodotto software resta in integrazione: mancano runtime autonomo e recovery qualificati, BLE/iPhone reali, persistenza definitiva, diagnostica restante, installer OTA/rollback, CloudKit live dopo membership e QA sul dispositivo. Nessun gate fisico è dichiarato chiuso da questi test.
