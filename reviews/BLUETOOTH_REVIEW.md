# Review indipendente APP07 — Bluetooth/RPC/stream

24 ottobre 2026. Sola lettura di `mobile/Sources/GaugeCore/Bluetooth`, `mobile/App/Bluetooth`, UI `BluetoothDevelopmentView` e suite `BluetoothStreamTests`/`BluetoothRPCTests`/`BluetoothGoldenRPCTests`. Coordinato con l’agente app: API e fix funzionali dichiarati stabili prima della prova. Nessuna modifica ai sorgenti mobile/shared durante la review; unico artefatto di review questo file. La separata integrazione autorizzata di ag_rpc.c nei CMake firmware e nel build report non cambia il codice recensito.

## Esito

**Rilievo di interoperabilità identificato dal coordinatore e risolto; nessun rilievo aperto nel perimetro esaminato.** I due problemi già identificati dal coordinatore risultano corretti: rifiuto apply definitivo limitato ai codici precommit `revision_conflict`/`invalid_configuration`, ed esito sconosciuto che blocca altre apply fino a rilettura config valida dopo hello/capability. Non vengono riproposti come nuovi rilievi.

## Interoperabilità riaperta e verificata — RISOLTO

La prima prova usava18test con pipe sintetico che aggiungeva sempre `op` alle risposte: non dimostrava compatibilità con gli exchange condivisi. Il coordinatore ha rilevato che config.current/validated/received/applied e alcuni rejected del golden originale omettono `op`, mentre il client lo richiedeva sempre. Questa discrepanza funzionale è stata corretta dall’agente app senza riscrivere il golden.

Review della correzione: `op` è opzionale nel tipo, ma una stringa presente deve coincidere esattamente con l’operazione pendente. Assenza ammessa soltanto per le forme legacy config-phase/rejected-error previste, con controllo delle chiavi consentite e correlazione frameID/operazione/fase. `op:null`, bool, numero, valore diverso, hello/logs-success senza op e campi estranei nelle forme legacy vengono rifiutati. Le risposte legacy non vengono arricchite con campi dal pipe.

Il replay end-to-end ora esegue **tutti i10exchange originali** di `shared/fixtures/rpc.exchanges.json`, via framing e chunk17byte: verifica gli ID1…10, le fasi, configurazioni, log e rifiuti, oltre all’uguaglianza dei payload trasmessi con i JSON caricati dalle fixture. I rifiuti applicativi validi completano la richiesta senza resettare il link/ID, permettendo lo scambio6→7; errori apply ambigui restano unknown. Prima di mantenere il link viene controllato che il rejected sia finale: frame extra o frammento parziale nella stessa consegna causano chiusura. Regressioni negative dedicate coprono anche questo confine.

## Prove riproducibili

```sh
swift test --package-path mobile \
  --scratch-path /tmp/auto-gauge-bluetooth-review-20261024 \
  --filter Bluetooth
```

**23/23 test superati**,12 RPC,6 stream e5 golden/negative, build/debug SwiftPM isolata fuori dal repository. Si tratta di test core con pipe finto, senza accesso a CoreBluetooth/radio; non dichiarati ASan/UBSan o prove su periferica reale. Il codice nativo è stato esaminato, non eseguito in questa review.

Copertura significativa verificata: frammentazione sul limite effettivo withResponse, ID richiesta crescente e nuova generazione, CCCD/readiness e limite scrittura zero, CRC/kind/ID/fase/protocollo errati, received→applied anche prima dell’ultimo ACK ATT, perdita ACK finale con commit già avvenuto, I/O ambiguo dopo received, rifiuto precommit noto, revisione inattesa, received mancante/solitario, timeout senza nuovi byte, cancellazione/disconnect e richiesta concorrente, mutazioni native chiuse anche quando hello dichiara authenticated=true.

## Lifecycle e confini controllati

- `GaugeRPCClient` mantiene una richiesta in corso. La richiesta usa una deadline assoluta condivisa da tutte le write e receive; il frame ha deadline dal primo byte, non rinnovata dai frammenti. Timeout/cancellazione chiudono il pipe e invalidano lo stato della richiesta. L’effettiva terminazione degli await richiede un adapter che sblocchi le continuazioni su cancel/disconnect; il nativo implementa entrambi i cancellation handler e azzera i waiter prima di riprenderli.
- Il nativo è MainActor, usa proxy delegate con generazione e identità del peripheral/characteristic. Vecchi callback/cancellazioni non possono risolvere i waiter di una generazione successiva nel normale lifecycle. Close cancella timer, subscription/riferimenti, decoder, code e waiter; riconnessione non riusa frammenti o request ID della vecchia istanza.
- Timer scan10s, connessione30s e frame10s sono bounded e non diventano retry automatici. Il timer di frame viene ricreato sulla stessa deadline originale; il controllo avviene all’arrivo anche mentre manca ancora l’ACK ATT dell’ultima write.
- Un frame incompleto è bounded4112byte, chunk512. Coda di frame completi validati separata≤4112byte; piena/corruzione/indicazione inattesa chiudono il link, senza troncamento o ricerca di magic nei payload. La coda non viene considerata prova di ricezione/applicazione RPC.
- L’ultimo chunk di apply può già causare commit prima del callback ATT: il percorso ambiguo conserva `applyOutcomeUnknown` anche dopo reconnect. Nessun rollback o retry automatico; config.get richiede prima capability da hello e valida il risultato prima di risolvere l’incertezza. I metadati ECU delle capability generiche restano unknown, non DEMO7E8.
- `CBCentralManager` nasce soltanto in `beginScan`, invocato dal pulsante con modalità sviluppo abilitata. Nessun costruttore a startup, restore cloud, fixture o lavoro in background. Il cambio vista/disabilitazione richiama stop. I dispositivi sono selezionati esplicitamente e la UI nativa espone soltanto hello.
- La subscription richiede caratteristica indicate senza notify concorrente e Write With Response; `isReady`, ATT e hello.authenticated non abilitano il gate immutabile delle mutazioni native. Le stringhe UI spiegano che hello non prova LESC/MITM/128bit/ammissione. Il percorso non viene presentato come server autenticato qualificato.

## Limiti che restano dichiarati

Non sono testati callback CoreBluetooth reali, background/sospensione iOS, perdita del controller, timing effettivo ATT o interoperabilità radio col gauge. Le prove non dimostrano negoziazione LESC/MITM, chiave128bit, identità/bond risolti o ammissione locale: tali requisiti restano server/stack da qualificare. Il client core offre altre operazioni per il pipe simulato, ma la UI nativa corrente non le espone e non esiste qui un’autorizzazione a renderle operative. Nessuna scansione, pairing, provisioning o comunicazione esterna eseguita durante la review.
