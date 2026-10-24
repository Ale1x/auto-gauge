# GATT di sviluppo v0.1

24 ottobre 2026. Profilo candidato implementabile e testabile; non un protocollo qualificato su radio. UUID/risorse/deadline sono scelte tecniche H, non nuovi requisiti di prodotto. Fonte machine-readable: `gatt.profile.json`. DEC-011/014 restano vincolanti: più telefoni sequenziali, un link attivo; codice locale per telefoni nuovi, riconnessione dei riconosciuti; nessun owner unico o iCloud obbligatorio.

## Trasferimento

Service custom e due caratteristiche definite nel JSON. Request: Write With Response, nessun Write Without Response. Response/event: indicazioni; il client deve attendere conferma di attivazione CCCD prima di inviare richieste. Conferma ATT del frammento significa soltanto accettazione ATT, mai `received`, `applied` o persistenza della configurazione.

Ogni direzione trasporta lo stream di frame `FRAMING_V0.1.md`: headerAG/version1/kind/requestID/length/reserved, JSONUTF8 eCRC32. Packet massimo4112byte, payload4096. Non aggiungere un secondo formato ad hoc. Chunk≤min(limite effettivo connessione/API,512); nessun MTU o payload20/512 dato per garantito. Sul server usare MTUeffettivo−overheadATT; su iOS `maximumWriteValueLength(for: .withResponse)`.

Un solo request in volo; requestIDnonzero crescente nella singola connessione. A esaurimento riconnettere, senza wrap nascosto. Risposte requestIDdiverso non soddisfano la richiesta corrente. `received` non conclude un apply: serve la successiva conferma `applied`/revisione/persistence o errore. Perdita risposta dopo invio completo lascia esito sconosciuto: rileggere config, non assumere rollback. Eventi requestID0 non sono risposte a richieste.

Un solo frame incompleto per direzione, buffer≤4112byte. Timeout assoluto dal primo byte, non prolungato da frammenti: H10s per BLE, diverso dal precedente H1s del riferimento host. Con MTUpiccolo e write confermate,1s non va assunto sufficiente per4KiB. Hrequest30s e scan10s sono parametri di test da misurare su hardware, senza garanzie di throughput. Timer deve scadere anche senza ulteriori pacchetti. Disconnessione/timeout/CRC/versione/kind/lunghezza errati svuotano stato e abortiscono transazione; nessuna ricerca magic dentro payload corrotto. Nessun frammento viene riusato su link/lease nuovi.

## Sicurezza e ciclo vita

- Il server richiede LESC,MITM,chiave128bit,cifratura e peeridentificato; il controller `ag_admission` deve consentire ogni lettura privata, write, indicazione e mutazione. Verificare anche stato effettivo del link su ogni invio, non soltanto permessi delle caratteristiche.
- Indicare cifratura perread/write non protegge automaticamente notification/indication. Perdita della sicurezza chiude il collegamento e invalida callback/lease/queue. Un client non autenticato non riceve configurazioni, log o metadati privati.
- CoreBluetooth non espone al client tutti i flag SMP del server; `hello.authenticated=true` non basta a dimostrare il livello crittografico. L’enforcement sullo stack server e i test con dispositivi sono indispensabili.
- Nuovo telefono: la persona apre il menu locale, il gauge mostra una passkey temporanea e iOS gestisce l’inserimento. Nessun codice fisso, trasmesso in advertising o loggato; nessuna cancellazione bond automatica per repeat-pairing remoto.
- Cachebond finita è un limite implementativo da gestire con nuova ammissione locale, mai con reset/cancellazione log o un Nmassimo di prodotto inventato. Identitàrisolta dal trustedstack, non MACricevuto inJSON.
- Client: scan esplicita/limitata,stop suconnect/cancel/background,unCBPeripheralattivo,generazione percallbacktardivi,cancel richieste disconnesse; nessuna scrittura in background o riconnessione infinita non richiesta.
- Il progetto può compilare adapter nativi mantenendoli opt-in e scollegati dalla modalità demo. Un adapter compilato non dimostra pairing/radio/device funzionante. Nessuna subscription Apple è necessaria per sviluppare CoreBluetooth; CloudKit resta separato.

Fonti primarie: [CoreBluetooth scritture](https://developer.apple.com/documentation/corebluetooth/cbperipheral/maximumwritevaluelength(for:)), [stato sottoscrizione](https://developer.apple.com/documentation/corebluetooth/cbperipheraldelegate/peripheral(_:didupdatenotificationstatefor:error:)), [ESP-IDFv6.1 SMP](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-guides/ble/smp.html). API e configurazione effettive vanno compilate/controllate sul toolchain fissato; non sono garanzie di sicurezza o conformità radio.
