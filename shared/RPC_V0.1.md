# Roundtrip applicativo di sviluppo

24 ottobre 2026. Messaggi JSON nel framing AGv1, endpoint esplicitamente simulato. Nessuna radio, socket, autenticazione reale o persistenza sul gauge. DEC-011 vale anche nel simulatore del collegamento: un solo client attivo, client diversi utilizzabili in sequenza senza reset.

## Configurazione

Richieste: `hello`, `config.get`, `config.validate` con `configuration`, `config.apply` con `configuration` ed `expected_revision`. Formato configurazione in PROTOCOL_V0.1.md. Tutte le risposte del riferimento riportano `simulation=true` e riusano il request ID della richiesta.

`config.get` restituisce `phase=current`, `configuration` e `persistence=volatile`. `config.validate` restituisce `phase=validated` oppure `phase=rejected` con `error`. `config.apply` produce prima `phase=received`, poi `phase=applied` con `applied_revision`/`persistence=volatile`, oppure `phase=rejected` con errore. La revisione attesa deve essere quella attuale e la nuova deve essere attuale+1. Campi extra non appartenenti allo schema1 non vengono conservati.

Il trasporto client deve verificare kind, request ID, fase e revisione, senza confondere conferma di ricezione e applicazione. Se la connessione cade prima del commit, il simulatore può annullare il pending. Se cade dopo il commit e prima dell'ACK, la configurazione del device resta cambiata: il client ha un esito sconosciuto e deve rileggere lo stato dopo riconnessione. **Un ACK mancante non dimostra che il commit non sia avvenuto.** Nessun rollback o retry cieco. Nel riferimento Python il commit è sincrono mentre vengono prodotti i due packet; un adapter può perdere il secondo packet dopo tale commit.

Le risposte pendenti del canale in memoria devono riferirsi al lease della connessione oltre al request ID: un vecchio messaggio A/id1 non può diventare una risposta di B/id1. Questo legame non introduce owner unico e non sostituisce la futura autenticazione.

## Storici

`logs.read` richiede `session_id`, `after_sequence` stringa uint64 canonica e `limit` intero1–16. Risposta: `{"simulation":true,"op":"logs.read","page":{...}}`; page segue LOG_SYNC_V0.1.md. Il limite4096byte comprende anche l'envelope: il produttore deve riservarne i byte o ridurre il numero di record, senza troncare record o JSON.

`logs.ack` richiede `session_id` e `acknowledged_through` come stringa uint64. Va inviato solo dopo l'import locale riuscito. Il riferimento risponde con `op=logs.ack`, stessa sessione/cursore e `source_records_deleted=false`; non elimina il buffer condiviso o conserva un registro di Ntelefoni. Una sessione diversa produce `error=session_mismatch`; sorgente non presente `history_unavailable`; parametri fuori limite `invalid_history_request`.

La sorgente volatile di riferimento non è il buffer flash. Nessuna cancellazione, login o sync cloud viene effettuata. La bozza salvata sul telefono, lo snapshot letto dal gauge e l'applicazione volatile confermata sono stati distinti nella UI.

## Estensioni verificate dall’endpointC

`firmware/DEVICE_ENDPOINT.md` collega il codec al controller e store effettivi. Le risposte config legacy possono omettereop e sono correlate tramite requestID/fase alla richiesta pendente; op presente deve corrispondere, hello/logssuccesso lo richiedono. Nessun campoOP viene aggiunto alle fixture per aggirare il contratto.

`persistence=durable` è ammesso soltanto con adapter che garantisce write durevole e readback: nelCLI significa filehost con fsync, non flashdevice qualificata. Il client lo accetta solo con policy esplicita; RAM/inmemory rimangono volatile, la UI nativa restahello-only.

Stato store non utilizzabile: `phase=diagnostic`, `error=configuration_not_operational` e `store_state` tra read_only/capability_mismatch/no_valid_configuration/conflict/io_or_integrity_error. Per lo storico read/ack: diagnostic/history_not_operational/read_only. Nessuna diagnostica diventa current o applied. Errore commit/readback ambiguo: `phase=outcome_unknown`, `error=outcome_unknown`, `store_state=commit_or_readback_failed`; non autorizza retry cieco né dimostra rollback.

Il client riconosce soltanto queste combinazioni per le operazioni appropriate; non allenta genericamente l’omissioneop. Diagnostica config non risolve una precedente apply incerta. Stringhe raw del backend restano dettagli tipizzati, con messaggi comprensibili nell’interfaccia.
