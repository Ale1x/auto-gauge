# Review indipendente — decoder C dello stream AG

24 ottobre 2026. Letti `shared/c/ag_stream.h`, `ag_stream.c`, `tests/stream_driver.c` e `test_c_stream.py`, confrontando la validazione con `ag_frame.h/.c`. Nessun sorgente modificato; nessuna lettura della APP-07 ancora in lavorazione, nessuna radio, firmware target o operazione remota.

**Esito: nessun nuovo difetto concreto rilevato nei percorsi esaminati, entro il contratto di stato posseduto da un solo task e callback non rientrante.** Il componente ricostruisce frame e controlla integrità; non autentica collegamenti o comandi.

## Verifica eseguita

`python3 -m unittest discover -s tests -p test_c_stream.py -v`: **1/1 test passato**, che compila ed esegue sei gruppi C con ASan/UBSan, warning pedanti ed errori abilitati. Inclusi tutti gli split di un frame campione, frame multipli, payload massimo, timeout, clock fault, header errati, CRC, callback rifiutato/rientrante e2048input pseudo-casuali bounded. Nessuna segnalazione dei sanitizzatori.

## Riscontri

- **Timeout assoluto:** `started_ms` viene assegnato soltanto quando inizia un frame con buffer vuoto. Nuovi frammenti non prolungano la scadenza. `observe` usa la differenza dopo controllo monotono, evitando overflow da somma deadline. Scadenza a `elapsed >= timeout_ms`, verificata anche da tick senza nuovi dati. Timeout scarta il parziale e il chunk che ha incontrato quell'errore: il chiamante deve ripartire con un frame intero.
- **Buffer e chunk:** memoria interna4112byte, chunk accettati1…512byte. Copia fino ai12byte dell'header prima di fidarsi della lunghezza, poi fino alla sola dimensione attesa. La lunghezza payload è limitata a4096prima di calcolare l'atteso. Il caso massimo è ricostruito in frammenti e decodificato senza crescita oltre il buffer.
- **Header e CRC:** magic, versione, kind, requestID coerente, riservati e lunghezza sono controllati già dopo12byte. Condizioni allineate a `ag_frame_decode`; quest'ultimo ricontrolla dimensione e CRC32 sull'intero frame prima del callback. Nessun resync cercando magic dentro un payload corrotto.
- **Consegna di frame distinti:** il callback del primo frame valido è definitivo per quel frame. Se il secondo nello stesso chunk ha CRC errato, ritorna INVALID_FRAME con `accepted=1`; il primo callback non viene richiamato né annullato. Il test lo verifica esplicitamente. Non esiste transazione dell'intero chunk.
- **Rifiuto del callback:** un handler che ritorna false interrompe la feed, pulisce i parziali e produce HANDLER_REJECTED; il frame rifiutato non incrementa accepted. Gli eventuali effetti già eseguiti dallo stesso handler non possono essere annullati dal decoder: il callback deve applicare la propria validazione/atomicità prima degli effetti irreversibili.
- **Clock fault:** regressione temporale pulisce il parziale e latchea CLOCK_FAULT. `reset` conserva quel fault e la storia temporale; solo `init` ricrea lo stato per un nuovo link. I test coprono valori vicini aUINT64_MAX e regressione a0, senza riuso silenzioso di tempo o wrap.
- **Rientranza:** feed e tick restituiscono BUSY durante il callback; reset viene rifiutato. Il flag feeding non è un lock atomico e non protegge chiamate concorrenti da task diversi. `init` è un costruttore che azzera lo stato, non un'operazione rientrante consentita: il contratto vieta al callback di richiamare qualunque API sullo stesso decoder, incluso init.

## Limiti da rispettare nell'adapter

1. Il payload del frame è **borrowed solo durante il callback**: dopo ritorno il buffer viene azzerato/riutilizzato. Copiare ciò che resta in coda e verificarne nuovamente lease/autorizzazione al dispatch; non trattenere il puntatore.
2. Chiamare tick con tempo monotono reale anche quando non arrivano chunk. Senza scheduling del tick, il solo decoder non chiude un parziale fermo finché non viene nuovamente chiamato.
3. Init di un nuovo collegamento non respinge da solo callback/chunk vecchi. L'adapter deve separare i lease e scartare eventi del link precedente prima di feed; reset elimina soltanto i byte.
4. Caller e callback devono rispettare validità/lifetime dei puntatori e possesso esclusivo dello stato. BUSY è protezione per rientranza sincrona, non prova di thread safety, né permette init dentro il callback.
5. CRC e header non costituiscono autenticazione o antireplay. L'adapter deve autorizzare ogni frame/operazione nel contesto attuale, senza dedurre trust da accepted o dal requestID.
6. Le prove host non misurano stack, latenza del callback, scheduler o limiti della radio sul target. Nessun controllo del decoder può annullare effetti di callback precedenti o recuperare automaticamente un chunk rifiutato.

Non è richiesta una correzione per chiudere questa review limitata. La successiva integrazione SDK deve mantenere questi contratti e distinguere ricostruzione del frame da autorizzazione del comando.
