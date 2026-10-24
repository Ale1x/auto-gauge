# Più client, una connessione BLE attiva — sviluppo v0.1

24 ottobre 2026. Vincolo approvato in DEC-011: più dispositivi utilizzabili in momenti diversi, una sola connessione Bluetooth attiva. Nessun owner unico del gauge e nessun reset per cambiare telefono. Gli altri dettagli di questa macchina a stati sono ipotesi tecniche da verificare.

## Connessioni e autorizzazione

- `idle`: nessuna connessione; può essere accettato un client.
- `connected_unverified`: slot occupato da un client, sessione non autenticata; nessuna mutazione reale autorizzata.
- `connected_verified`: un adapter di sicurezza esterno ha verificato la sessione corrente; non è una proprietà esclusiva persistente del gauge.
- Un secondo tentativo mentre lo slot è occupato restituisce busy e non sostituisce il client attuale.
- Alla disconnessione lo slot torna disponibile; un altro telefono può collegarsi senza reset. La nuova sessione riparte non verificata.
- Ogni evento deve riferirsi al collegamento corrente tramite identificatore/lease di sessione. Un callback tardivo del vecchio collegamento non disconnette né autentica quello nuovo. Esaurimento del contatore non causa wrap o riuso silenzioso.

Il core di sviluppo non ha un registro con un solo telefono ammesso. Bonding e identità autorizzate sono responsabilità del futuro adapter BLE/sicurezza; non viene fissato N numerico nella policy. L'API di test che riceve un esito di verifica non implementa crittografia, non prova la provenienza dell'esito e non va esposta come un comando remoto.

I cursori storico appartengono a ciascun telefono. Confermare l'import da A non elimina i record disponibili per B sul gauge: il buffer rimane condiviso e bounded, con sovrascrittura dei più vecchi secondo DEC-006. Il cambio di client non azzera configurazione, buffer o associazioni degli altri client. Il reset fisico volontario di manutenzione ha invece gli effetti di cancellazione locale già approvati.

La configurazione del gauge è condivisa: al collegamento un client deve leggere lo stato/revisione corrente prima di applicare una bozza locale. L'operazione di commit deve verificare la revisione attesa, così il telefono B non sovrascrive silenziosamente modifiche apportate da A dopo l'ultima lettura di B. Il simulatore applicativo implementa già questo controllo; la revisione della bozza iPhone e una conferma di commit sul gauge restano eventi distinti.

## Reset fisico separato

Il reset locale richiede USB valida e conferma fisica qualificata dal futuro HAL; USB inserita da sola o un messaggio BLE non bastano. Durante il reset si rifiutano connessioni/mutazioni e si invalida la sessione attuale. La procedura cancella credenziali/bond locali, configurazione e log del gauge; non contatta il cloud né elimina storici già sincronizzati.

Il core deve modellare un intent di reset persistito prima delle cancellazioni, avanzamento dei singoli passaggi e commit finale. Un'interruzione o fallimento non deve far riapparire vecchie credenziali: al reboot uno stato incompleto resta bloccato finché la procedura è completata. Una nuova connessione diventa possibile solo dopo la conclusione del reset, poi segue la normale autenticazione di sessione. Non esiste trasferimento a un nuovo owner unico.

Tempi, gesto, debounce, anti-reset accidentale, protezione della memoria e atomicità flash reale non sono definiti da questa policy. Lo scaffold non cancella dati del dispositivo e non tocca eFuse o chiavi. Il reset non autorizza traffico CAN: stato quadro/alimentazioni e guardie diagnostiche restano indipendenti.

## Prove minime su host

1. A si collega; B riceve busy, senza sostituire A.
2. A si disconnette; B si collega senza reset o cancellazione; poi può tornare A.
3. Una verifica valida per A non rende verificato B; callback/token vecchi non hanno effetto sulla sessione nuova.
4. Connessione non verificata non autorizza mutazioni; nessun esito di test è presentato come autenticazione reale.
5. USB sola, evento remoto o conferma fisica non qualificata non avviano reset.
6. Ogni fase di reset interrotta conserva lo stato bloccato al reboot; nessuna sessione precedente torna valida.
7. Reset completo cancella solo stato locale secondo il contratto di manutenzione; il core non contiene API per eliminare il cloud.
