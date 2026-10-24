# Associazione esclusiva — decisione O-08 da prendere

24 ottobre 2026. Proposta al proprietario, nessuna opzione approvata o implementata. La V1 richiede un solo iPhone associato alla volta, ma non specifica chi possa sostituirlo se viene perso. Il prossimo trasporto autenticato dipende da questa scelta.

## Opzione A — recupero fisico locale, proposta consigliata per il prototipo

Il gauge conserva un solo legame di controllo. Per sostituire un iPhone perso serve il possesso del dispositivo e una procedura fisica di manutenzione via USB/pulsanti, ancora da verificare sul circuito. La procedura revoca il vecchio legame e cancella dal gauge credenziali, configurazione e log locali, poi consente una nuova associazione. Non richiede di contattare il cloud per autorizzare il reset.

Non cancella automaticamente account e storici già sincronizzati nel cloud; quella cancellazione appartiene al servizio account. Il controllo fisico permette il recupero anche senza il telefono precedente, ma chi ha il dispositivo in mano può ripristinarlo. Non è un sistema antifurto. Modalità manutenzione, gesto di conferma e protezione contro reset accidentale vanno progettati; non si assume che collegare un cavo USB basti da solo.

## Opzione B — proprietà legata all'account

La proprietà è registrata nell'account cloud e la sostituzione dell'iPhone richiede autorizzazione dell'account, oltre alla prova di possesso del dispositivo secondo una procedura da definire. Un semplice reset fisico non trasferisce la proprietà. Il normale funzionamento del gauge rimane autonomo e offline, ma recupero/trasferimento dipendono da backend, rete e recuperabilità dell'account.

Richiede progettare insieme servizio ownership, revoca dei telefoni, recupero account, trasferimento/vendita e comportamento durante indisponibilità cloud. È più complesso e non può essere simulato come una semplice password locale.

## Cosa non viene deciso da questa domanda

Provider cloud, minimo iOS, primitive crittografiche, chiavi e provisioning, backup, retention e cancellazione account restano separati. Non si bruciano eFuse né si cancella alcun dato scegliendo una policy. Entrambe le opzioni richiedono implementazione, threat review, autenticazione/antireplay e prove di revoca prima di abilitare comandi reali.

## Criteri futuri di verifica

Un secondo iPhone non modifica il gauge mentre il primo è owner; un vecchio legame revocato non torna valido dopo reboot; il recupero non accetta un semplice messaggio BLE; le interruzioni durante il cambio owner non lasciano due owner o credenziali parziali; nessun trasferimento di ownership autorizza attività CAN a quadro spento. Finché manca la decisione e il protocollo verificato, i trasporti reali restano bloccati.
