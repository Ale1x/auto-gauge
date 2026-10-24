# Lettura del coordinatore — FW08 persistente simulato

24 ottobre 2026. Letti `ag_flash_history.h`, `flash_history.c` e il contratto `FLASH_HISTORY_MODEL.md`, con confronto al serializzatore `ag_history`. Nessuna modifica ai sorgenti. Review del modello, non della flash fisica.

Nessun nuovo difetto concreto individuato nei percorsi esaminati. Geometry/offset/capacity sono bounded prima degli accessi; il contatore globale e le sequenze per SID restano distinti; exhaustion impedisce wrap. Il commit tocca soltanto il banco inattivo, valida header/record/padding prima del marker banco e ripete il readback dopo il marker. Un errore dopo la scrittura completa può comunque rendere visibile lo snapshot nuovo al riavvio: il chiamante deve rileggere, senza assumere rollback.

Bootstrap richiede esplicitamente tutta la media erased. Open non scrive o adotta il SID precedente come boot corrente; begin_session deve ricevere un ID nuovo fidato e preserva i record precedenti. La funzione rifiuta SID corrente/trattenuto ma non può ricordare tutti gli ID già totalmente evictati: freshness del generatore resta obbligo del caller. I record normali non sono resi unici dal solo timer.

La scansione verifica header, CRC/marker record, serial globale contiguo, sequenza/tempo per run e coerenza della sessione corrente. Con due banchi validi, regressioni dei metadati sono conflitti. Un banco invalido/illeggibile impedisce scritture anche se l’altro è leggibile; pagine READ_ONLY sono diagnostiche e possono essere precedenti. Dove retention/cursore non sono dimostrabili non si presenta un falso NOT_RETAINED o una pagina vuota completa. Chi invia sul wire deve controllare il codice risultato, non soltanto la presenza di JSON nel buffer.

Pagine ricostruite su una finestra bounded di16record e serializzate col core comune. Il cursore successivo è la copertura effettiva della pagina; il chiamante continua finché riceve una pagina vuota sana, senza dedurre fine archivio dal numero di record o dal limite in byte. ACK consulta ma non cancella o mantiene un cursore globale del telefonoA; il telefonoB può rileggere.

## Limiti sostanziali

- **Non è il backend V1.** Copia ed erase di mezzo store a ogni append: O(capacità), throughput/endurance inadatti a un log operativo senza redesign. Due banchi dimezzano lo spazio utile;2MiB consentono8190record128byte, circa27min18s sotto la sola ipotesi5campioni/s. Nessuna promessa di7ore o budget OTA concluso.
- **Un power loss può lasciare READ_ONLY e bloccare nuovo logging.** Non c’è manutenzione automatica sicura di un banco sporco: è una scelta conservativa esplicita, non autonomia V1 dopo crank già dimostrata. I campioni non committati/overflow coda sono perdite distinte dagli overwrite persistiti.
- **Stack significativo.** Lo snapshot fixed-capacity6200byte resta sullo stack del chiamante anche se popolato con16record; la serializzazione aggiunge staging4097byte. I frame statici Xtensa riportati superano11KB annidati prima di caller/helper. Servono task separato/lock, misure runtime e margine; nessuna chiamata dal main inerte.
- **CRC non autentica.** Nessuna resistenza a riscrittura ostile, rollback fisico o generazioni ricostruite con CRC valido. IO deve rispettare durabilità sincrona e singolo owner, senza modifiche concorrenti.

L’agente firmware riporta9/9CTest con sanitizzatori,2267tagliI/O, bitflip4096byte e corrispondenza byte-per-byte delle6fixture/12pagine comuni; il coordinatore ha rieseguito con successo queste prove nel comando integrato (9/9CTest, con ASan/UBSan). Questa lettura non le trasforma in qualifica elettrica o prove di power loss fisico.
