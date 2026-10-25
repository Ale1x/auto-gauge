# APP-15 — recovery degli intenti storici qualificati

25 ottobre 2026. Solo software locale; nessun cloud live, radio, flash o modifica del gate nativo.

## Ordine dei commit

Il percorso qualificato chiama l'entry point interno `LocalLogArchive.importQualifiedPage`. Il normale `importPage` pubblico, usato da simulatori/fixture, non crea intenti cloud.

1. Decodifica e valida pagina, record e perdite; prepara il nuovo cursore.
2. Nello stesso snapshot archivio v2 aggiunge gli elementi cloud qualificati e verifica che ogni elemento corrisponda ai record/perdite realmente conservati.
3. Un solo `storage.write` atomico sostituisce record, cursore e intenti. Soltanto dopo il successo si può inviare l'ACK al gauge.
4. `recoverQualifiedImports(to:)` raggruppa gli intenti per device/sessione e li consegna all'inbox con receipt interno sigillato e deduplica per recordID/contenuto.
5. Dopo il commit inbox rimuove dall'archivio soltanto gli ID consegnati. Nuovi intenti aggiunti durante un await restano nell'archivio.

Un crash prima della consegna riparte dagli intenti dell'archivio. Un crash dopo il commit inbox ma prima della pulizia ripete una consegna idempotente. Un errore di write può seguire una sostituzione riuscita: archivio/inbox invalidano lo stato caricato e richiedono riconciliazione dal file prima di scrivere ancora. Una nuova generazione di reload invalida la pulizia di una recovery precedente.

## Integrazione app

Un unico actor archivio nativo vive per tutta l'istanza AppModel, anche attraverso connessioni sequenziali/disconnessioni e ritorno al simulatore. All'avvio l'app rilegge l'archivio nativo e recupera gli intenti senza dover connettere il gauge o selezionare un account. Il reload storici e ogni import qualificato riprovano la consegna; gli errori restano visibili e i dati locali conservati. Il trasferimento dall'inbox all'outbox dell'account richiede ancora l'azione esplicita già prevista dalla UI.

## Compatibilità e limiti

- Archivio v1 leggibile: viene migrato in memoria a v2 con intenti vuoti. Non esiste prova locale sufficiente per promuovere retroattivamente i vecchi dati a fonte qualificata; un vecchio crash già avvenuto non viene falsamente dichiarato recuperato.
- Formato v2 non retrocompatibile con vecchie app. Nessuna modifica automatica dei file originali avviene al solo reload.
- Stessi limiti archivio: 512 KiB, 1024 record, 16 sessioni, 128 intervalli di perdita. Gli intenti pendenti sono al massimo 1024; i byte duplicati nel medesimo snapshot concorrono ai 512 KiB. Capacità insufficiente rifiuta il commit prima di un nuovo ACK.
- File locali e sostituzione atomica Foundation: prove di interruzione processo ed errori prima/dopo commit modellati, non una qualifica di fsync/power-loss fisico del telefono. Nessun locking fra processi; un actor proprietario per file nell'app.
- La provenienza sigillata protegge il percorso API applicativo, non il tamper fisico dell'archivio locale. Nessuna qualifica Bluetooth viene ricavata da `hello` o dal contenuto dei file.
- Il gate `UnqualifiedNativeGaugeProvider` resta predefinito. CloudKit live e interop fisico restano non provati.

## Verifiche

I test APP15HistoryRecoveryTests coprono restart prima dell'inbox, failure inbox prima/dopo commit, failure pulizia archivio prima/dopo commit, import dall'esito ambiguo senza overwrite successivo, migrazione v1 e separazione simulata, intenti orfani rifiutati e file scomparso che non conserva una proiezione RAM obsoleta.

APP15CHostCatalogTests usa un vero subprocess C compilato dal repository con ammissione fixture esplicita, framing/codec/RPC C reali e catalogo della sessione effettivamente conservata. Controlla SID iniziale, cursore esclusivo, cambio sessione e rimozione di quella non più conservata. Non usa un server Swift che imita la risposta C e non dichiara qualifica radio.
