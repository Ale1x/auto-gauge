# Sessioni e reset — invarianti di sviluppo

24 ottobre 2026. Applicazione tecnica di DEC-011, non implementazione crittografica o approvazione di sicurezza. Più client possono usare il gauge in sequenza; l'esclusività riguarda soltanto la connessione attiva. Non è previsto un trasferimento di proprietà fra telefoni.

| Evento/rischio | Invariante richiesta | Portata delle prove host |
|---|---|---|
| A è collegato, B tenta la connessione | B ottiene busy; il collegamento A non viene sostituito | Verifica della macchina a stati serializzata; il futuro stack BLE deve configurare un solo link e gestire l'esito del rifiuto |
| A si disconnette, B entra | B parte non verificato e usa un nuovo lease; nessun dato o configurazione viene cancellato | Nessuna dipendenza da reset o vecchio telefono |
| Callback di A arriva dopo B | Identificatore/lease non corrispondente non cambia lo stato B | Il lease correla eventi, non autentica un client o impedisce da solo replay sul canale |
| Client non verificato occupa lo slot | Nessuna mutazione; il futuro adapter deve gestire timeout e rate limit, da dimensionare | Il solo limite di un link non risolve denial-of-service |
| Due callback arrivano in concorrenza | Core chiamato da task unico o sotto lock; nessuna doppia accettazione | I test seriali non costituiscono prova di thread safety |
| Conferma fisica di reset | USB valida, evento fisico qualificato e conferma locale; un messaggio remoto non bastano | I booleani/eventi di test non sono misure elettriche o autenticazione del canale |
| Scrittura intent reset fallisce | Nessun erase prima dell'intent durevole; non inviare ACK di reset completato | Atomicità flash e journal reale non sono implementati |
| Power loss dopo intent e prima del commit finale | Restore lascia reset incompleto/fallito e sessioni bloccate; riprendere cancellazione in modo idempotente | Occorre un backend persistente reale con validazione di integrità e prove su ogni finestra d'interruzione |
| Journal corrotto o restore non verificabile | Fail closed; il boot non pubblicizza BLE prima di ricostruire lo stato sicuro | La validazione di uno snapshot C non prova resistenza a rollback fisico o corruzione flash |
| Reset completo | Credenziali/bond, configurazione e log locali cancellati; cloud non toccato | Nessuna chiamata cloud o eFuse deve essere introdotta dal core |
| A conferma import log | ACK di A non cancella il buffer condiviso richiesto da B; cursori sui telefoni | Buffer volatile di riferimento e due archivi locali, non ancora flash device |

Il futuro adapter deve verificare identità e freschezza della sessione prima di dichiararla verificata; non può derivarlo dalla connessione, dal request ID o dal CRC. La sostituzione del client non richiede factory reset. Non esiste un registro di un solo iPhone ammesso: i limiti tecnici dei bond non sono un nuovo N di prodotto e richiedono verifica della piattaforma.

Le guardie CAN restano indipendenti da connessione, autenticazione e reset: collegare un telefono o USB non prova IGN ON e non autorizza richieste diagnostiche. Il target di standby non è dimostrato dal core software. Non sono generate chiavi, attivate radio o cancellati dati reali in questa iterazione.
