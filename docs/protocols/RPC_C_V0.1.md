# Codec RPC C bounded — sviluppo v0.1

24 ottobre 2026. `shared/c/ag_rpc.h/.c` aggiungono parsing tipizzato di richieste e codifica di un insieme esplicito di risposte del contratto `docs/protocols/RPC_V0.1.md`. Sorgenti originali senza dipendenze esterne, heap o vendoring. Il parser configurazione esistente non è modificato: lo scanner RPC trova il suo span e lo consegna a `ag_config_json_parse` con la maschera capability del chiamante.

**Non è un server RPC o un controller di dispatch.** Non apre radio, confronta lease, autentica, applica configurazioni, legge/cancella log o produce ACK dopo persistenza. Kind/request ID del framing, identità e sessione corrente, antireplay e autorizzazione sono responsabilità del dispatcher futuro. Nessuna operazione CAN raw, servizio diagnostico arbitrario, reset, owner, OTA o cloud è accettata.

## Richieste

```c
ag_rpc_request_t candidate;
ag_rpc_error_t error;
ag_rpc_result_t result = ag_rpc_request_parse(
    payload, payload_length, &supported_pids, &candidate, &error);
```

Input length-delimited, nessun NUL richiesto. Il chiamante deve passare oggetti validi e accesso serializzato; error/output/input e campi di controllo non devono sovrapporsi. Il risultato `candidate` cambia solo su OK: su qualunque errore resta byte per byte immutato. `error` è facoltativo, offset bounded; gli errori di configurazione conservano anche risultato/dettaglio del parser e del validatore core. Nessun puntatore all'input viene trattenuto.

| Operazione | Campi esatti oltre `op` | Tipo risultante |
|---|---|---|
| `hello` | nessuno | `AG_RPC_HELLO` |
| `config.get` | nessuno | `AG_RPC_CONFIG_GET` |
| `config.validate` | `configuration` | configurazione C candidata validata con capability |
| `config.apply` | `configuration`, `expected_revision` | configurazione più revisione attesa1…INT32_MAX |
| `logs.read` | `session_id`, `after_sequence`, `limit` | sessione16byte, cursoreuint64, limite1…16 |
| `logs.ack` | `session_id`, `acknowledged_through` | sessione16byte e cursoreuint64 |

I campi devono essere presenti esattamente per l'operazione selezionata. Proprietà note ma appartenenti a un'altra operazione sono rifiutate, così come proprietà sconosciute. L'ordine è libero; `op` può anche essere ultimo. Una revisione sintatticamente valida non viene confrontata con uno stato attivo dal codec: per esempio una richiesta stale del golden resta una richiesta valida, che il controller deve respingere come conflitto. Il parsing di un ACK non prova che il telefono abbia scritto i record e non li elimina.

`sequence` nella struttura rappresenta `after_sequence` oppure `acknowledged_through`, secondo operation; non passa mai attraverso Double. La sessione è esattamente32cifre esadecimali minuscole non tutte zero. Il cursore è una stringa decimale canonica da0 aUINT64_MAX, senza segno/zeri iniziali o frazioni. La maschera PID non viene dedotta dal catalogo demo e non prova da sola la disponibilità di decoder o di un veicolo.

## Grammatica e memoria bounded

- Massimo4096byte per **payload**, compresi whitespace ed envelope; nel framing AG il limite totale resta4112byte con16byte di overhead. Il NUL dell'encoder non fa parte del payload.
- Un solo oggetto root, grammatica di separatori completa per i tipi supportati; niente BOM, NUL, commenti, trailing data/comma. Whitespace soloSP/TAB/CR/LF.
- Profondità massima8container, massimo512token contando aperture, chiavi e scalari. Lo scanner della configurazione è ricorsivo solo entro questo limite, poi il parser di schema esistente controlla duplicati, tipi e cardinalità. Non si alloca un albero di token né una copia dell'input.
- Stringhe RPC ASCII stampabile, massimo64byte decodificati; sono supportati `\"`, `\\`, `\/` e `\uXXXX` quando risolvono in ASCII stampabile. Non ASCII, surrogate e controlli raw/escaped sono rifiutati esplicitamente. Le chiavi vengono decodificate prima del rilevamento duplicati: `op` e `\u006fp` non possono coesistere. I campi della configurazione restano soggetti al limite31byte del parser di schema.
- Interi numerici lessicali signed32; niente float/esponenti/booleani/null/numeri in stringhe dove è richiesto un numero. I due cursori sono invece deliberatamente stringheuint64. Vincoli semantici ulteriori vengono applicati per campo.
- Chiavi extra **rifiutate**, anche dentro la configurazione. È un sottoinsieme più stretto delle vecchie proiezioni Swift/Python che scartavano extra. Interoperabilità dimostrata sul normale schema e fixture, non promessa su documenti estesi o Unicode arbitrario.

Il parser conserva una richiesta candidata, stato di lettura e piccoli buffer stringa per i livelli bounded; nessun heap/VLA. La profondità del normale schema RPC è7container. Stack e tempo effettivi sul target devono essere misurati nella futura integrazione.

## Risposte tipizzate disponibili

`ag_rpc_reply_encode` serializza un `ag_rpc_reply_t` in staging locale da4097byte. Solo a codifica completa e capacità sufficiente copia nell'output. Su errore output immutato e `written=0`; su successo `written` esclude NUL e la capacità deve includerlo. Non si accettano frammenti JSON raw, quindi il chiamante non può superare il budget concatenando envelope e pagina.

Disponibili: hello con identità/capability/versione1 e flag authenticated esplicito; configuration/current; validated; received; applied con applied_revision; rejected con errore da enum finita; logs.ack con sessione/cursore e `source_records_deleted=false` fisso, più logs.read da snapshot tipizzato (sotto). Nessuna risposta modifica stato.

`simulation`, `authenticated`, fase e persistence sono **asserzioni esplicite del chiamante**, non deduzioni da parsing o presenza BLE. Per current/applied la persistence deve essere impostata: UNSPECIFIED viene rifiutata. `volatile` riproduce il riferimento simulato; `durable` è una proposta tecnica di etichetta esplicita per un futuro commit persistente verificato, non una garanzia prodotta dal codec né un'estensione già congelata per tutti i client. Prima di usarla sul canale, coordinare il contratto dei client e l'effettiva semantica del backend. Il codec può rappresentarla nei test senza avere effettuato alcuna write.

La configurazione di current viene validata strutturalmente con tutti i PID numerici ammessi: può servire per una UI di diagnosi/riparazione anche se il veicolo corrente non la supporta. Questo non abilita l'applicazione della configurazione, che deve usare le capability attuali.

Errori codificabili: revision_conflict, session_mismatch, history_unavailable, invalid_configuration, invalid_history_request, not_implemented, unauthorized, invalid_request, invalid_message, unsupported_pid. La scelta del codice e il relativo significato di stato restano al dispatcher.

### Risposta logs.read con snapshot tipizzato

Impostare `kind=AG_RPC_REPLY_LOGS_READ`, `history` a uno `ag_history_snapshot_t` stabile, `session_id`, `after_sequence`, `limit`1…16 e `simulation` esplicito. Il codec chiama esclusivamente `ag_history_make_page` del firmware: nessuna API accetta una pagina JSON arbitraria. Lo snapshot non viene modificato o consumato, e l'operazione non emette ACK né conserva cursori dei telefoni.

Il writer costruisce il prefisso RPC nello staging da4097byte. Al serializzatore interno passa la capacità residua **meno un byte**, così la pagina conserva il proprio NUL temporaneo e rimane spazio per la chiusura dell'envelope e il NUL finale. L'intero payload, inclusi simulation/op/page e l'ultima parentesi, resta≤4096byte. Il NUL non è trasmesso.

Se una pagina valida internamente supera il budget dell'envelope, il codec riprova con un limite record decrescente, al massimo16tentativi, fino a trovare una pagina intera. Non tronca record, intervalli o JSON. È distinto dal limite interno4096byte del produttore: una pagina che entra da sola può essere troppo grande nell'RPC. La pagina può contenere meno record del limite richiesto; il chiamante riprende dal record/intervallo effettivamente coperto, non dal limite numerico richiesto. Snapshot e campi devono restare immutati per tutta la codifica.

Sorgente invalida produce `AG_RPC_HISTORY_INVALID`, sessione diversa `AG_RPC_SESSION_MISMATCH`, cursore oltre l'ultima sequenza `AG_RPC_CURSOR_AHEAD`; limite fuori1…16 è ARGUMENT. Nessuna risposta rejected viene inventata automaticamente: il dispatcher sceglierà l'errore wire appropriato. Errori, impossibilità di budget o capacità caller insufficiente lasciano output immutato e written0. Un output caller corto non causa un adattamento silenzioso della pagina: occorre normalmente un buffer da4097byte.

Sono gestiti pagina vuota, sola perdita, sequenza terminaleUINT64_MAX, raw fino64byte e cursori uint64, secondo le invarianti del produttore tipizzato. Il loop e il serializer richiedono staging annidato: circa4KiB del writer RPC oltre ai4KiB interni del produttore, più strutture/stack del chiamante. Non collocare automaticamente questo percorso nel task app_main predefinito: misurare stack e durata sul target. Nessuna garanzia di schedulazione o persistenza viene dedotta dalle prove host.

## Verifica riproducibile

```sh
python3 -m unittest discover -s tests -p test_c_rpc.py -v
```

Compilazione C11 con warning pedanti, ASan/UBSan e assert attivi in directory temporanea. Il driver verifica output invariato su ogni rifiuto e written/offset bounded. Undici gruppi Python confrontano:

- tutte10richieste golden con i valori C tipizzati, compresi stale apply e sessione diversa validi nel parser ma da verificare nel controller;
- tutti22casi configurazione in entrambe validate/apply, più capability vuota;
- tutte12risposte golden, incluse le due pagine logs.read prodotte da snapshot C e confrontate come oggetti JSON con Python;
- required/extra/duplicate/unknown e alias di chiavi viaASCIIescape, ordine libero, nessuna op arbitraria;
- stringheuint64 fino aUINT64_MAX e oltre2^53, overflow e forme non canoniche, sessioni invalide, revisioni/limit di tipo errato;
-4096/4097byte, nesting/token excess, JSON malformato, Unicode fuori subset e duplicati nella configurazione;
- risposte con output corto/esattamente senza spazioNUL, persistence mancante, enum/sessione/identità e snapshot invalidi, session mismatch e cursore oltre il termine;
- tutte12pagine delle6fixture condivise di paginazione, più raw64byte e tempi/sequenzeUINT64_MAX, resume senza perdita o duplicazione, pagina vuota/sola perdita e caso di bordo in cui15record entrano nel produttore ma solo14 nell’envelope; payload esatto4096byte accettato e pagina equivalente da4097byte ridotta senza troncamenti;
- ogni troncatura e mutazioneNUL/0xFF dei due request campione, più1024input pseudo-casuali bounded per campione.

Sono prove host del codec, non un fuzzing esaustivo, un'interoperabilità BLE reale o una review crittografica. Nessun cambiamento a firmware/mobile, fixture o parser configurazione esistente, e nessun commit/remoto eseguito.
