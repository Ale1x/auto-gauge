# APP-05 — review indipendente CloudKit e outbox

24 ottobre 2026. Sola lettura di `mobile/Sources/GaugeCore/Cloud/`, relativi test e adapter `mobile/App/Cloud/`; verificati anche call site UI, archivio locale, tipi/decoder storico e README. Nessuna modifica a `mobile/` o `shared/`, nessuna chiamata CloudKit, provisioning, account o operazione remota eseguita. La review si riferisce al nuovo incremento CloudKit Development; non ripropone la decisione sul provider già approvata in DEC-012.

## Rilievo concreto trovato e corretto dall'agente app

**P2 — un callback vecchio poteva invalidare il nuovo account selezionato.** La prima versione di `CloudHistoryOutbox.synchronize` eseguiva `invalidateAccount()` sul risultato diverso di `await database.account()` prima di controllare generation e contesto correnti. Sequenza: sync di A sospesa su `account()`, selezione di B durante la sospensione, callback vecchio che restituisce B; il vecchio task cancellava il contesto B. Le code persistenti restavano conservate, ma il nuovo account perdeva la selezione e il suo stato visibile.

Segnalato direttamente all'agente proprietario, senza modificare i suoi file. La versione attuale usa `verifyAccount`: attende il risultato, controlla cancellazione e token/contesto, poi soltanto un task ancora corrente può invalidare un mismatch. Helper usato nei tre punti del ciclo. L'agente ha inoltre aggiunto controllo cancellazione/epoch UI dopo il reload, prima della selezione.

Regressione `testStaleAccountCallbackCannotInvalidateNewlySelectedContext`: fake `account()` sospeso, selezione e inserimento nella coda B, rilascio callback del task A; B resta selezionato, la sua sync riesce, la coda A rimane. **Rilievo risolto nella versione riesaminata.** Non emergono nuovi P0/P1 nei percorsi letti.

## Verifiche indipendenti

Atteso il punto stabile comunicato dall'agente app, quindi eseguito `swift test --package-path mobile --scratch-path /tmp/auto-gauge-cloud-review.CRdVez --filter CloudHistoryTests`. Primo snapshot:10 test passati. Dopo correzione e nuovo punto stabile, suite cloud rieseguita: **11 test passati, zero errori**, inclusa la regressione. Questa suite compila il package GaugeCore: non include `mobile/App/Cloud/` né dimostra funzionamento CloudKit nativo. Build simulatore e device unsigned sono riportate dall'agente app, non rieseguite da questa review.

## Riscontri sui confini richiesti

| Tema | Riscontro concreto |
|---|---|
| Perdita dati / receipt locale | Enqueue e marcatura uploaded preparano uno snapshot candidato, lo validano e lo scrivono prima di sostituire quello attivo. Errore di write conserva pending e snapshot corrente; errore di reload conserva memoria/file e blocca nuove operazioni dipendenti dal caricamento. ACK del gauge è assente dall'API cloud e resta distinto. |
| Commit remoto riuscito, receipt persa | Il pending non viene rimosso o considerato annullato. Retry trova identità immutabile già esistente e richiede contenuto identico. I test coprono scrittura locale fallita dopo remoto, riapertura, account cambiato dopo commit remoto e cancellazione del task. Nessun presunto rollback server. |
| Cambio account | Code indicizzate per contesto, selezione distinta, token generation invalidato al cambio/reload. Controlli dopo gli await impediscono receipt locale del task precedente. La correzione sopra impedisce anche al callback precedente di invalidare una selezione nuova. A→B non copia i dati di A nella coda B. |
| Idempotenza / conflitti | Record ID SHA256 di device/session/sequence o estremi della perdita, separato dal digest del payload. Stessa identità e contenuto uguale è idempotente; contenuto diverso conserva pending e non sovrascrive. Il ciclo nativo legge l'esistente o crea con `ifServerRecordUnchanged`, riconciliando `serverRecordChanged` solo se payload e metadati coincidono. |
| Callback / cancellazione nativa | `CloudSaveCompletion` serializza result/continuation con lock e risolve una sola volta anche nella gara fra callback e cancellazione. La cancellazione conserva pending; non è una promessa che un record già ricevuto dal server venga eliminato. Nessuna prova con SDK remoto è stata eseguita. |
| Fixture | I call site dell'app non passano `AppModel.logSessions` a enqueue. L'API rifiuta `.simulation`; test sintetici usano soltanto fakeDB. Non esiste ancora un produttore da radio reale, quindi questo incremento non carica automaticamente lo storico dimostrativo. L'enum di provenienza è un contratto del chiamante fidato, non autenticazione crittografica della sorgente. |
| Codable / contatori | Envelope schema1, device ID bounded, sessione128bit valida, esattamente uno fra record e perdita. Record/perdita riusano inizializzatori validanti; sequenze e tempi restano stringhe uint64 canoniche, senza Double. Percorsi di ingresso della coda e `CloudHistoryItem.decode` eseguono controllo JSON stretto e limiti prima dell'uso. Identità non cambia al variare del contenuto: il caso viene rilevato come conflitto. |
| Capacità |512KiB,1024 elementi complessivi,8 contesti; il limite comprende uploaded. Nessuna eviction silenziosa dei pending. Il raggiungimento del limite rifiuta nuovi elementi e preserva archivio/cloud. È un limite del prototipo, non una policy di retention definitiva. |
| Abilitazione del servizio | Factory nativo disabilitato sul simulatore e nella configurazione unsigned predefinita; richiede opzione esplicita e profilo incorporato Development con identità/servizio/container previsti. Il parsing del profilo è gating applicativo, non verifica crittografica della firma: quella resta a iOS/provisioning. |

Il comportamento della save policy è coerente con la [documentazione primaria Apple](https://developer.apple.com/documentation/cloudkit/ckmodifyrecordsoperation/savepolicy): un change tag incompatibile deve produrre conflitto anziché sostituire la copia server. La [notifica di cambio account Apple](https://developer.apple.com/documentation/cloudkit/ckaccountchangednotification) richiede una nuova verifica dello stato; non fornisce, da sola, una prova di atomicità fra controllo account e richiesta remota.

## Limiti aperti, senza trasformarli in bug già dimostrati

- **Upload e riconciliazione puntuale, non sync completa.** Nessun download per ricostruire lo storico su un secondo telefono, merge remoto completo, cancellazione account/dati o retention finale. Una receipt uploaded non viene riconfermata a ogni sync: se qualcuno cancella successivamente il record cloud, il prototipo non lo scopre automaticamente. Non descrivere lo stato come backup verificato permanente.
- **Account switch del sistema e richieste SDK in volo non sono qualificati dai fake.** Pre/post check e cancellazione impediscono false receipt locali nei casi modellati, ma non provano da soli il legame atomico dell'operazione CloudKit con l'account di sistema durante ogni transizione. Serve test con profilo valido, due account e interruzioni reali prima di una dichiarazione di isolamento remoto completa. Non è stata osservata una scrittura nell'account sbagliato e non viene affermato che esista.
- **Conflitto blocca il ciclo, senza risoluzione distruttiva automatica.** Il comportamento è dichiarato e conserva dati; la futura UI di diagnosi/riparazione non deve risolverlo sovrascrivendo il server o scartando pending.
- **Persistenza locale di un actor per file.** Nessun coordinamento fra processi/istanze multiple, fsync/power-loss fisico, policy definitiva di data protection/backup iOS o migrazione di schema. Le prove su storage in memoria e atomic replacement non dimostrano tutti questi aspetti.
- **Contenitore/schema/profilo e dispositivo reali non provati.** Compilazione e fakeDB non dimostrano entitlement effettivi, schema server, quote, errori di rete CloudKit o capacità di provisioning. La review non ha attivato il servizio né richiesto nuove autorizzazioni.

Conclusione limitata: base outbox conservativa e testabile, con la race P2 corretta durante la review. Nessuna perdita silenziosa o sovrascrittura remota riprodotta nei percorsi host; CloudKit live e completamento dei requisiti cloud restano da verificare.
