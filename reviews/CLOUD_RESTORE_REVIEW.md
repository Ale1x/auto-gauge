# APP-06 — review indipendente del recupero storico CloudKit

24 ottobre 2026. Sola lettura dei cinque tipi core pull/restore, `CloudRestoreTests.swift`, adapter nativo/query/completion, modello account, viste di recupero e README. Nessuna modifica a mobile/shared, nessuna chiamata live, provisioning o commit. Atteso il punto stabile dell'agente app prima dei test.

**Esito: nessun nuovo rilievo P0/P1/P2 riproducibile nei percorsi esaminati.** Il recupero è un archivio locale incrementale di osservazioni cloud, non una replica completa o una fotografia atomica del database remoto.

## Verifica indipendente

Eseguito:

```sh
swift test --package-path mobile --scratch-path /tmp/auto-gauge-cloud-review.CRdVez --filter CloudRestoreTests
```

**13/13 test passati**, zero errori. Comprendono checkpoint e riapertura, errore di write, conflitto nella seconda pagina, digest/identità, cursore scaduto, cambio account durante read/account, cancellazione, limite pagine/record/file e coesistenza di perdite e record da telefoni diversi. L'agente app riporta inoltre66 test complessivi e build simulatore/device unsigned riuscite; questa review ha rieseguito soltanto la suite dedicata. GaugeCore non compila l'adapter CloudKit nativo: la sua lettura e la build dell'app non equivalgono a test di servizio reale.

## Riscontri sui rischi richiesti

| Rischio | Evidenza concreta |
|---|---|
| Checkpoint avanzato senza record | `CloudHistoryRestoreStore.restore` costruisce `merged` con tutti gli elementi della pagina, cursore e stato finale; li inserisce in un solo snapshot candidato e chiama `persist`. Snapshot attivo sostituito soltanto dopo write riuscita. Conflitto o errore su qualsiasi record respinge l'intera pagina e lascia il vecchio checkpoint. |
| Retry dopo interruzione/offline | Le pagine precedenti già salvate restano disponibili. Dopo riapertura si riparte dal cursore persistito. Duplicati con stessa identità/contenuto non vengono aggiunti di nuovo; contenuto diverso produce conflitto senza avanzare. Il test della write fallita verifica che la richiesta successiva ripeta la stessa pagina. |
| Cursore scaduto | Un errore non cancella record o checkpoint. La nuova scansione è esplicita: azzera cursore/stato finale e conserva gli elementi esistenti. L'adapter tratta fallimento di unarchive sicuro e alcuni errori server con cursore come `cursorExpired`; non ricomincia silenziosamente perdendo lo stato. |
| Query non snapshot | `scanFinished` significa soltanto assenza di nextCursor in quella traversata. Una chiamata normale successiva non presume nuovi dati; la UI offre una nuova scansione e avvisa che completezza e freschezza non sono garantite. Il test aggiunge dati dopo il termine e li trova solo con restart. |
| Cambio account durante await | Ogni operazione conserva token generation e contesto. Dopo read e dopo verifica account controlla cancellazione/token prima di scrivere. La precedente race APP-05 non viene ripetuta: callback account vecchio non può invalidare il contesto nuovo. Code e checkpoint non migrano tra account. |
| Cancellazione | Dopo una read sospesa, cancellare prima del commit conserva il precedente snapshot. I callback query sono coordinati da lock e result unico, inclusa cancellazione; non viene persistita una pagina parziale arrivata prima di errore/cancel. Restano da testare i callback SDK reali. |
| Dati da altri telefoni e perdite | Il restore non usa l'importer gauge che respinge record dentro perdite già riconosciute. Conserva la perdita come osservazione della sorgente insieme ai record disponibili da altri telefoni. Nessuna cancellazione di record o falsa dichiarazione di assenza globale. La UI esplicita questa semantica. |
| Identità e conflitti | Elementi identificati da device/session/sequence o estremi della perdita. Device/session differenti restano distinti. Stessa identità con contenuto diverso blocca tutta la pagina; un duplicato identico è idempotente. Non sovrascrive elementi precedenti. |
| Digest e schema | Ogni payload è bounded, controllato con SHA256, decodificato con il decoder rigoroso schema1, poi confrontato con recordID ricalcolato. L'adapter nativo controlla anche tipo record e schemaVersion del wrapper. SHA256 rileva incoerenza del contenuto, non autentica il dispositivo che lo ha prodotto. |
| Bounds | Massimo16 record/pagina,4096byte/payload,64KiB complessivi della pagina conteggiando payload+ID+digest,64KiB separati per il cursore. Snapshot massimo512KiB/1024elementi complessivi/8account, massimo8pagine per azione. Raggiungere un limite conserva il precedente stato e il checkpoint. Cursore identico al precedente viene rifiutato senza commit. |
| Codifica | Sequence/tempo uint64 restano stringhe canoniche; fixture oltre2^53 e fino aUInt64.max passano senza Double. Snapshot ricaricato è validato per conteggi, identità duplicate, cursore non vuoto/bounded e impossibilità di scanFinished insieme a cursore presente. Nessuna data assoluta inventata. |

## Verifica delle assunzioni CloudKit

La [documentazione Apple di init(cursor:)](https://developer.apple.com/documentation/cloudkit/ckqueryoperation/init%28cursor%3A%29) precisa che il cursore rappresenta un offset relativo: la richiesta successiva esegue una nuova ricerca, non riprende uno snapshot congelato, e sconsiglia di conservarlo a lungo. Il codice e i testi attuali rispettano questa distinzione. Il cursore persistente è un tentativo di resume, non una garanzia di completezza.

La [documentazione Apple resultsLimit](https://developer.apple.com/documentation/cloudkit/ckqueryoperation/resultslimit) definisce il limite dei risultati per richiesta; il codice aggiunge propri limiti di byte e record, fermando l'intera pagina se vengono violati. Non inventa un numero di record che debba sempre arrivare e accetta pagine vuote con cursore diverso.

## Limiti da mantenere espliciti

1. **Una scansione può saltare o ripetere elementi se il risultato remoto cambia.** La deduplica risolve le ripetizioni, non può ricostruire ciò che la query non ha restituito. Restart può migliorare la copertura, senza promessa di replica esatta. Non sono implementati change token di zona, snapshot del server o propagazione delle cancellazioni remote. Gli elementi locali già recuperati restano anche se successivamente spariscono dal cloud.
2. **Il cursore opaco può scadere o diventare inutilizzabile per altri errori SDK.** Gli errori attualmente classificati e il fallback esplicito sono ragionevoli, ma la tassonomia completa e la sopravvivenza dell'archivio del cursore attraverso aggiornamenti OS richiedono prove reali. Un codice server non classificato lascia lo stato conservato e mostra errore, senza garantire il messaggio specifico «cursore scaduto».
3. **Le prove di atomicità sono del percorso locale e dello storage fake.** Sostituzione atomica del file non dimostra ogni power loss fisico, data protection/backup iOS o coordinamento fra processi/actor multipli sullo stesso file. Il contratto resta un actor proprietario per file.
4. **Isolamento account remoto non qualificato sul sistema reale.** Pre/post check e cancellazione proteggono i commit locali nei casi modellati; non provano da soli atomicità fra account di sistema e operazione CloudKit in volo. Servono profilo valido, due account e transizioni reali. Nessuna fuga di dati nell'altro account è stata osservata o qui affermata.
5. **Archivio cloud separato, non recovery completa del prodotto.** Non ricrea configurazione dashboard, credenziali, bonding o flash gauge; non emette ACK al gauge. Nessuna fixture demo viene promossa a storico reale dal percorso nuovo. Questo incremento conserva record e osservazioni di perdita ricevuti, non ne certifica l'origine veicolo.
6. **Native callbacks e provisioning restano fuori dai test host.** Le classi di query/completion sono state lette, ma non esercitate contro CloudKit. Disponibilità del container/schema, entitlement reali, quote, rete e comportamento degli errori server non sono provati da queste13fixture.

Nessuna correzione richiesta per chiudere questa review limitata. Il risultato aggiunge un percorso di recupero testato con checkpoint conservativo; non autorizza dichiarazioni di backup cloud completo o qualificato live.
