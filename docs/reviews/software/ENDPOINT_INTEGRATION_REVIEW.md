# Review del coordinatore: endpoint e integrazione

24 ottobre 2026. Perimetro: dispatcher C, store configurazione, contratto RPC e collegamento al bridge NimBLE. Review mobile separata in docs/reviews/software/APP08_REVIEW.md; lifecycle radio simulato in docs/reviews/software/NIMBLE_STUDY_REVIEW.md.

La lettura e le regressioni hanno portato a tre correzioni:

- **Configurazione precedente promossa a current dopo corruzione recente.** Uno slot non erased ma invalido ora impone READ_ONLY. La copia valida può servire alla diagnosi, senza diventare configurazione operativa/durabile confermata. Anche una scrittura interrotta può richiedere manutenzione: non è ancora recupero automatico V1.
- **Timestamp di un vecchio lease applicato al peer successivo.** Attach e guardie confrontano boot/counter prima del tick, anche dopo I/O. A obsoleto non può provocare clock fault su B o cancellarne l’output.
- **Lettura file dopo fsync fallito.** Startup e restore-io richiedono un nuovo fsync riuscito prima di rimuovere il fault; leggere la sola pagecache non giustifica current/durable.

Il dispatcher mantiene received e applied distinti; un errore durante commit/readback resta outcome_unknown. Il bridge aspetta la conferma EDONE dell’intero frame received prima di procedere al commit e ricontrolla tempo/autorizzazione dopo I/O. I test con I/O lento verificano che un commit già avvenuto non produca una risposta scaduta. ACK dei log non cancella dati condivisi.

Nessun rilievo concreto rimasto aperto nel perimetro esaminato. Questa conclusione non qualifica sicurezza radio, flash fisica, power loss, stack operativo o recupero automatico. Il processo host usa ammissione fidata simulata, dichiarata esplicitamente; il main target resta inerte. Il backend flash storico COW non è ancora adatto alla V1.
