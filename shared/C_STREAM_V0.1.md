# Stream C bounded — riferimento v0.1

`shared/c/ag_stream.{h,c}` ricostruisce i packet AG del codec esistente. Nessuna allocazione heap, frame incompleto massimo4112byte, chunk1–512byte, header verificato appena completi12byte e CRC controllato sul packet completo. Il buffer è parte del contesto: non allocarlo implicitamente sullo stack del callback BLE senza misurare lo stack completo.

Il chiamante fornisce un timeout positivo; il profilo GATT candidato usa H10000ms. La scadenza è assoluta dal primo byte del frame e non viene rinnovata da ogni frammento. `ag_stream_tick` va chiamato dal timer anche senza input. Sottrazioni temporali ordinate evitano overflow del deadline aUINT64_MAX. Regressione del clock azzera i byte e blocca il contesto finché non viene inizializzato nuovamente; `reset` da solo non nasconde il fault.

Errore header/CRC, timeout o chunk invalido scarta la ricostruzione; nessuna ricerca della magic nei byte residui. Una nuova connessione deve avere un nuovo contesto inizializzato/lease; il parser non può distinguere da solo callback della connessione precedente. Non usare il semplice reset come autenticazione o protezione replay.

## Callback e atomicità

La callback riceve una vista presa in prestito sul frame, valida soltanto durante la chiamata. Deve copiare dati da conservare e verificare il contesto/sessione autorizzati prima di qualsiasi effetto. Ogni frame completo è una transazione indipendente: se un chunk contiene A valido seguito da B corrotto, la callback diA può essere già eseguita quando feed segnala l’errore diB. Il contatore `accepted` riporta le callback già accettate; non promettere rollback degli effetti precedenti.

Il riferimento Python e il core Swift accumulano invece un array locale prima di restituirlo: un errore successivo può impedirne la consegna al chiamante. È una differenza nell’interfaccia locale di streaming, non un diverso formato wire o una garanzia di rollback del device. Una configurazione già applicata resta da rileggere se si perde la risposta.

Un solo owner serializzato; nessuna thread safety. Feed/tick/reset ricorsivi durante una callback sono respinti senza cambiare il buffer. Init/reinizializzazione e modifiche dirette al contesto sono vietati dentro la callback. Chunk, contesto e contatore output non devono aliasarsi. Il contesto non trattiene puntatori all’input.

## Prove

`python3 -m unittest discover -s tests -p test_c_stream.py -v` compila con ASan/UBSan e warning-as-error. Sei gruppi verificano ogni punto di split, più frame nello stesso chunk, massimo4096byte con frammenti20byte, timeout senza traffico/non esteso, reset/clock regression/UINT64_MAX, header/CRC/chunk invalidi, callback rifiutata/reentry, effetti parziali espliciti e2048input pseudo-casuali bounded. Non è fuzzing esaustivo né prova di radio o autorizzazione RPC. L’integrazione target viene compilata senza chiamate dal main inerte.
