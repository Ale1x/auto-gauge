# Endpoint C integrato — sviluppo host

24 ottobre 2026. `ag_device_endpoint.h/device_endpoint.c` collegano stream/frame AG, RPC tipizzato, ammissione, configurazione a doppio slot e backend storico bounded. **Main inerte, nessun GATT/CAN/flash hardware/eFuse attivato.** Il CLI è una fixture host esplicitamente simulata, non un server Bluetooth autenticato qualificato.

## API e lifecycle

1. Inizializzare admission e config store separatamente, poi `ag_device_endpoint_init` con opzioni e callback read-only dello storico. Nessun bootstrap/default o apertura radio nell’endpoint.
2. La persona/stack deve completare l’ammissione tramite adapter fidato; `attach(lease,now)` richiede che `ag_admission_private_allowed` sia già vero. Non riattaccare lo stesso lease per azzerare gli ID.
3. `feed(lease,bytes,length,now)` accetta chunk1…512 e verifica autorizzazione prima di leggere frame o payload. Solo request kind, ID nonzero strettamente crescente per lease, RPC/schema/capability validati. Un solo typed request in corso; niente servizi arbitrari CAN, owner, cloud o reset remoto.
4. `step(now)` esegue una fase. `take_output(lease,now,buffer)` emette una sola risposta framed≤4112byte, tutto o niente, ricontrollando lease/security. Il trasporto reale dovrà ricontrollare la stessa autorizzazione prima di **ogni frammento fisico**, conservare il lease dell’output e gestire completamento/errori ATT; take è un handoff, non conferma di consegna.
5. Disconnect invalida il lease di admission e cancella lavoro non committato/code/partial. Eventi di un lease precedente restituiscono STALE senza cancellare il peer corrente. Attach e le guardie di operazione/emissione confrontano boot/counter con il link admission corrente prima di qualunque tick/auth: un vecchio callback A con timestamp1 non può clock-faultare B già autenticato a100, nemmeno nel controllo post-I/O se B è subentrato durante il commit. Busy su una nuova feed separata non sostituisce la richiesta già pendente; due request coalesced nello stesso chunk abortiscono prima dello step con effetti applicativi.

Hello è protetto come configurazione, log e relativi metadati: prima di ammissione non viene inviata nemmeno una risposta d’errore con metadata. I frame/CRC/ID non sono autorità. La guardia LESC/MITM/128bit deriva dagli hook admission fidati, non da un booleano del comando remoto. Eventi, backend e invio richiedono un owner task/lock; i callback non devono rientrare nell’endpoint.

Opzioni temporali H: CLI frame10000ms e request30000ms. La deadline richiesta parte dal primo byte, comprende parsing/fasi/code e non viene estesa dai frammenti; step/take controllano anche richieste tipizzate o risposte ferme in coda. Il chiamante runtime deve chiamare step/tick senza aspettare nuovi byte. Dopo I/O e prima di take_output deve campionare di nuovo il clock monotono: non riciclare il timestamp precommit. step è sincrono e usa il suo snapshot temporale; non è una misura della durata del backend. Il nuovo istante al confine di emissione/tick successivo applica la deadline reale anche se l’I/O è durato a lungo. La CLI usa poll/deadline anche su stdin parziale. Tempo monotono uint64 fornito dal caller, regressioni falliscono chiuse. Non è una misura di throughput o timing radio.

## Configurazione: received, commit e incertezza

Apply viene prima parsata/validata e produce `received`; il commit non può iniziare finché quel frame non è stato consumato e non viene chiamato lo step successivo. La cancellazione/disconnessione in mezzo lascia la configurazione precedente. Prima di scrivere vengono riletti store, revisione attesa, candidata e guardia attuale. Revision-conflict/invalid-configuration sono rifiuti precommit; received non significa applicazione.

`applied` richiede `ag_config_store_commit == OK`, restore/readback sano e confronto canonico esatto della candidata con la configurazione riletta. L’asserzione persistence viene dal backend fidato: RAM=volatile, adapter file CLI=durable solo dopo write/fsync/readback. Un errore dopo l’ingresso nel commit è conservativamente `phase=outcome_unknown,error=outcome_unknown`, mai un rejected definitivo o rollback inventato. Un disconnect durante/dopo I/O può lasciare la candidata persistita ma blocca qualsiasi emissione al lease precedente. Perdita dell’ultimo ACK/app-response richiede rilettura, non retry cieco.

Config.get produce current/persistence soltanto da restoreOK, writable e compatible. READ_ONLY/capability-mismatch/no-valid-config/conflict/I/O danno `phase=diagnostic,error=configuration_not_operational,store_state=...`, senza falso current/durable. Anche uno slot non-erased invalido con altra copia integra è READ_ONLY: il fallback potrebbe essere più vecchio della copia recente danneggiata. L’endpoint non ripara/cancella automaticamente tali copie; una manutenzione sicura resta da progettare e non è qualificata autonomia V1 dopo crank.

## Storico

Il backend restituisce una copia tipizzata `ag_history_snapshot_t`, valida e indipendente, con capacità≤64 e SID/cursore verificati. L’encoder RPC riserva il budget dell’intero envelope4096byte e può ridurre il numero di record. Lo storico non viene consumato. ACK viene verificato sullo snapshot; non esiste un callback erase/ACK mutante né una mappa globale dei cursori di N telefoni. Un backend degradato espone diagnostica, non una pagina completa falsa. Il backend flash FW08 non è collegato automaticamente: per quel modello occorre un adapter di finestra tipizzata dedicato, senza riparsare JSON di pagina.

## CLI binaria — soltanto fixture fidata

```sh
cmake --build firmware/build-host --target gauge_device_endpoint_cli
firmware/build-host/gauge_device_endpoint_cli \
  --trusted-fixture-admission --seed-initial \
  --store-file /tmp/gauge-config.bin --control-file /tmp/gauge-control.txt
```

Stdin/stdout contengono **solo frame AG binari**, non linee hex o JSON di controllo. Il flag trusted-fixture è obbligatorio: gli hook dichiarano un link sicuro simulato legato a peer/lease, non verificano alcuna negoziazione radio. Hello dichiara `simulation=true,authenticated=true,device_id=SIMULATO-001` soltanto dopo questi hook; authenticated in questo processo non è prova crittografica e non va promosso a fiducia dal client reale. La passkey del controller usa entropia OS e non viene stampata, pubblicizzata o salvata. Nessun codice fisso nel CLI, nessun dato privato su stderr.

Con store-file, due slot8192byte sono salvati con pwrite mirato allo slot modificato e fsync; readback rilegge il file. Un fsync riuscito è richiesto anche prima di caricare un file esistente al nuovo processo e prima di clear restore-io: leggere la sola pagecache dopo un precedente fsync fallito non autorizza current/durable. Se il fence fallisce resta errore I/O, senza default. Non si riscrive l’intero file a ogni mutazione rischiando lo slot attivo. `--seed-initial` è esplicito e serve soltanto per file assente/totalmente erased, usando `shared/config.valid` rev1; non sovrascrive un file corrotto. Ometterlo al riavvio dimostra restore. Senza store-file la configurazione è RAMsim e le risposte persistence sono volatile. Nessuna chiave/bond reale o configurazione utente esistente viene toccata dai test.

Il processo parte come peerA della fixture; A/B sono associabili/riconosciuti in sequenza, non owner unici. La memoria bond è un mock tecnico limitato ai due peer di test, non il massimo N di prodotto. Lo storico CLI è un ring RAM cap4, SID `05050505050505050505050505050505`, sequenze3…6, PID12/ECU0x7E8, tempi2000…5000ms e raw1a10, allineato al golden. Non è uno storico flash persistente; riavvio processo ricrea questa fixture.

Il sidecar di controllo è **fuori dal protocollo**, accessibile solo al test host con quel path. Scrivere un solo comando ASCII seguito da LF prima del prossimo frame; viene consumato/troncato quando inizia il nuovo packet. Non esiste un’op RPC equivalente:

| Comando | Effetto della fixture |
|---|---|
| connect-a / connect-b | Chiude vecchio lease, connette peer indicato, nuova sequenza ID da1; config e ring invariati |
| disconnect | Invalida il lease; una richiesta successiva non passa finché un connect non è applicato |
| drop-applied | Il prossimo applied viene consumato senza stdout; received resta visibile, commit già persistito |
| fail-next-io | Errore sulla prossima operazione store, latched fino a restore-io o restart |
| fail-after-marker | Dopo il marker commit durevole/fsync provoca errore di readback: outcome_unknown, possibile nuova revisione su disco |
| restore-io | Richiede fsync(fd) riuscito, poi toglie il fault I/O; fallimento resta latched, nessun rollback/config rewrite |
| disconnect-during-commit | Invalida admission alla prima programmazione; commit può completarsi, nessuna risposta al vecchio lease |
| new-log-session | Sostituisce solo il ring RAM della fixture con SID0606… e stessi quattro campioni; non è erase remoto o persistenza multi-sessione |

Codici uscita:2 argomenti/flag fixture mancante,3 startup/backend,4 protocollo/controllo/trasporto. Il controller sidecar non è un dispatcher di produzione e non deve essere esposto via GATT o rete.

## Memoria, stack e prove

Un solo stream4112byte, pending typed request e output4112byte; endpoint host circa9KiB, niente heap nel core. Backend snapshot fixed-capacity e encoder creano stack annidato importante. GCC Xtensa con vera riga IDF `-Og`/`-fstack-usage`: perform6304byte, queue_reply4144byte, RPC encode4160byte, history_page96byte e history serializer4752byte; i frame si sommano lungo logs.read oltre19KiB prima di libc/caller/adapter. Non è high-water o un task budget sufficiente. Lo stack default main non è qualificato; il main non chiama questi moduli e il linker GC li elimina dallo skeleton, quindi `.bin` non dimensiona il firmware operativo o OTA.

```sh
ctest --test-dir firmware/build-host --output-on-failure
python3 firmware/tools/check_endpoint_interop.py \
  --executable firmware/build-host/gauge_device_endpoint_cli
```

Test C dinamici: guardia prima di parsing/emissione, BUSY/replay/stale/A→B→A, output corto non consumato, received separato, cancellazione precommit, ACK perso e readback, errore dopo marker, disconnect durante commit senza vecchia emissione, ACK storico senza consumo, SID nuovo, capability mismatch diagnostico e deadline senza byte.

L’interop Python esegue tutti10exchange originali sia RAM sia file (20scambi): variazioni **esplicite** solo authenticated=true per hookfixture e persistence=durable per adapter file. Il golden non viene riscritto e le risposte non vengono arricchite con op. Copre inoltre A/B con ACK indipendenti, newSID, riavvio file senza seed, last-response persa, post-marker I/O, readonly dopo corruzione recente e rifiuto bootstrap su doppia corruzione. Nessuna comunicazione radio, provisioning, firmware flash o update Git effettuati.
