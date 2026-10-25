# Account e cloud — CloudKit scelto in DEC-012

Il proprietario ha scelto account iCloud del telefono e CloudKit, senza account email separato. L'opzione Supabase sotto è conservata come confronto storico e non va implementata. La configurazione Apple e le prove live restano da eseguire.

Confronto storico del 24 ottobre 2026, precedente a DEC-012: la tabella seguente documenta le alternative valutate. Il provider ora è scelto; non occorre riproporre questa decisione. La disponibilità operativa del container resta distinta dalla scelta del provider.

| Opzione | Comportamento per l'utente | Dipendenze concrete |
|---|---|---|
| Account iCloud + CloudKit, consigliata per il perimetro solo iOS | L'app usa l'account iCloud presente sull'iPhone, senza creare un account email/password separato. L'archivio cloud privato è dell'utente iCloud | Container/entitlement CloudKit e firma Apple da configurare; account iCloud disponibile per la sync. I dati privati consumano quota iCloud dell'utente |
| Account dedicato all'app + Accedi con Apple, backend gestito Supabase | L'utente accede esplicitamente all'app con Apple; il backend conserva i dati separati per account, senza richiedere storage iCloud del telefono | Progetto Supabase, provider Apple e configurazione developer, regole di accesso e policy dati. È un servizio esterno ulteriore da gestire |

La seconda opzione può usare AuthenticationServices e URLSession senza aggiungere framework Swift di terze parti. Non si introduce un SDK esterno automaticamente. Non sono dichiarati costi o gratuità garantiti; prima di risorse a pagamento occorrono piano e limiti concordati.

## Regole comuni da preservare

- Il gauge funziona senza telefono/cloud. La mancata sync non deve bloccare telemetria o occupare memoria illimitata sul gauge.
- DEC-011 resta invariata: più telefoni possono usare il gauge in sequenza, una connessione BLE attiva. L'account cloud non crea un owner unico del gauge.
- I cursori importati rimangono locali ai telefoni; sync cloud e ACK verso gauge sono eventi diversi. La cancellazione della manutenzione gauge non cancella account/storici cloud.
- L'archivio deve mantenere le sequenze unsigned64 senza conversione a Double o int64 signed; usare rappresentazioni stringa/chiavi ordinate adeguate. Deduplica device/sessione/sequenza e conflitti espliciti.
- Token/chiavi restano fuori dal repository. Il deploy e le prove reali richiedono l'account developer/configurazione del provider: una build con mock non dimostra servizio cloud funzionante.

Retention, esportazione/cancellazione account, visibilità dei dati e distribuzione restano da finalizzare prima del rilascio. DEC-012 sceglie il meccanismo di account; DEC-013 autorizza la configurazione Development. Restano escluse pubblicazione dell’app e spese.

Fonti primarie consultate: [Apple CloudKit privateCloudDatabase](https://developer.apple.com/documentation/cloudkit/ckcontainer/privateclouddatabase), [Apple CKContainer](https://developer.apple.com/documentation/cloudkit/ckcontainer), [Supabase Sign in with Apple](https://supabase.com/docs/guides/auth/social-login/auth-apple).
