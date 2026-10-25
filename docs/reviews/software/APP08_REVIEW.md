# Review indipendente APP08 — transport di sviluppo e processo C

24 ottobre 2026. Lettura di `mobile/Sources/GaugeCore/Development`, RPC client/response/persistence-policy/endpoint-failure types, `CHostEndpointSupport`, `CHostEndpointTests`, `DevelopmentTransportTests` e regressioni Bluetooth. Nessuna modifica mobile da questo reviewer; le correzioni sotto sono dell’agente app. L’autore del CLI C verifica qui **il client e il protocollo d’integrazione**, non si assegna un’approvazione indipendente della sicurezza del proprio server C. La review C endpoint resta al coordinatore.

## Esito

**Nessun rilievo aperto nel perimetro esaminato.** Tre punti emersi nella review sono stati corretti e verificati con regressioni dedicate: diagnostica tipizzata, handoff con vecchia read/process ancora pendenti ed exception-safety del sidecar di test. Nessuna radio, pairing, provisioning, comunicazione remota o commit eseguiti.

## Prova riproducibile

```sh
swift test --package-path mobile \
  --scratch-path /tmp/auto-gauge-app08-review-20261024 \
  --filter 'CHostEndpointTests|DevelopmentTransportTests|Bluetooth'
```

**39/39 test superati** sul punto stabile finale:10 processo C,2 transport unificato,9 golden/diagnostica,12 RPC e6 stream. Il subprocess C è compilato dai test in una directory temporanea con `AG_SANITIZERS=OFF`; questa prova Swift non va descritta come ASan/UBSan o test radio. Le suite C sanitizzate e la build target hanno evidenze separate. Dopo l’ulteriore fence fsync richiesto dal coordinatore nel CLI (restore-io e startup file esistente), i12test processo/unified sono stati ripetuti:12/12 verdi, compresi errore post-marker e rilettura nello stesso PID.

## Punti corretti durante la review

**R1 RISOLTO — diagnostica legittima senza falso current.** Inizialmente `diagnostic`/`outcome_unknown` del C venivano respinti come invalidReply generico: conservativo rispetto al commit, ma perdeva la causa storage. Ora `GaugeEndpointFailure/Kind/StoreState` riconoscono solo forme coerenti per operazione, fase, errore, chiavi e stato enum. Unknown store-state, op errato/null, campi successo mescolati o fase sbagliata non diventano current/applied. Apply diagnostico/ambiguo conserva il latch unknown e `lastEndpointFailure`; reconnect/hello o config.get READ_ONLY non lo risolvono. Solo current qualificato dalla policy/capability lo risolve. La prova sul processo C corrompe la copia recente e verifica diagnosi read-only, cache precedente non promossa a current e nessuna apply automatica.

**R2 RISOLTO — vecchio reader e processo prima del successore.** Il precedente flag reading globale poteva attraversare la generazione e stop non attendeva l’uscita del vecchio writer. Il supporto test ora usa readingGeneration, readCount registrato prima del reader detached e FD chiuso una sola volta dopo la sua uscita. `confirmStopped` attende process-exit e zero reader entro2s; se non confermati non avvia il successore. La reservation connecting impedisce due connect concorrenti mentre attende. La regressione avvia una read A pendente, disconnette e collega subito B: nuovo PID/file restore, nessun vecchio ID1 consegnato e nessuna sovrapposizione dei writer prevista dal lifecycle. Questi sono vincoli del harness macOS, non una prova di callback CoreBluetooth.

**R3 RISOLTO — errore sidecar non lascia lease irraggiungibile.** Prima active/generation venivano pubblicati prima della write control; un errore filesystem faceva perdere il lease al pipe lasciando il hub busy. Ora il comando viene scritto prima di spawn/pubblicazione. La regressione usa una directory al path controlFile, verifica fallimento senza nuovo PID, rimuove l’ostacolo e ripete connect/get revision1 con successo. Impatto limitato alla fixture di test; nessun nuovo comando remoto.

## Proprietà verificate

- `volatileDevelopment` accetta soltanto persistence volatile; `hostDurableStore` soltanto durable. La policy è immutabile/localmente scelta, non deriva da hello.authenticated o da un campo remoto. Il backend in-memory non può soddisfare il profilo host-durable. Il label client non è di per sé una prova di fsync: la configurazione esplicita del test e il processo C controllato costituiscono quel contesto.
- Il processo C reale usa il file8192byte e dimostra apply/readback dopo restart senza seeding implicito. Perdita applied RPC e I/O dopo marker restano unknown finché la rilettura qualificata verifica la revisione; nessun rollback o retry cieco. Non è una prova di ACK ATT fisico, perdita alimentazione o storage flash del gauge.
- `DevelopmentGaugeTransport` richiede lettura prima dell’apply dopo connect/reconnect. Mantiene separati bozza del telefono, configurazione osservata e revisione assegnata dal peer. Stale-revision viene respinta senza sostituire snapshot/config; A→B→A pulito conserva un solo processo e rilegge la revisione nuova prima di mutare.
- Import storico: `LocalLogArchive.importPage` costruisce/valida la candidata, scrive atomicamente, aggiorna snapshot/cursore e soltanto dopo restituisce ACK. Il transport lo invia dopo tale ritorno. Write locale fallita non invia ACK né modifica lo snapshot; rimosso l’errore reimporta gli stessi record. Un ACK non recapitato dopo commit non annulla l’import locale. Archivi A/B hanno cursori indipendenti e il C non cancella record dopo ACK.
- Pipe host trasporta i byte AG originali, non arricchisce op né riscrive risposte. I decoder del hub servono a logging/correlazione e controllo del pending; le write inoltrano i data originali. Replay dei10exchange condivisi e test negativi op/null/extra/partial restano verdi.
- Process/compilazione CLI/fixture-admission sono esclusivamente in `Tests` sotto `#if os(macOS)`. I sorgenti app/core non includono quel launcher. `NativeBluetoothController` non conforma a DevelopmentGaugePacketPipe, costruisce RPC con gate mutazioni predefinito chiuso e la UI nativa resta hello-only. Il bootstrap dell’app collega il simulatore in-memory, non avvia scansione radio.

## Limiti dichiarati

Il percorso è esplicitamente locale di sviluppo. Fixture admission e hello.authenticated non attestano LESC/MITM/128bit, bonding/identità o ammissione sul dispositivo reale. Test file/fsync host non qualificano flash/endurance/rollback del gauge; `.atomic` dell’archivio telefono dimostra il percorso di commit/errori testato, non resistenza a una perdita fisica di alimentazione del telefono. Nessuna prova di background/sospensione iOS, negoziazione ATT, radio o hardware. La manutenzione dopo storage READ_ONLY e il backend flash storico completo restano incrementi distinti.
