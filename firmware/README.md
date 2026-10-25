# Firmware Auto Gauge — base di sviluppo v0.1

Nucleo C11 e applicazione ESP-IDF per il candidato ESP32-S3, sviluppabili prima della qualifica di pannello, alimentazioni e quadro. La build nativa compone gli adapter reali; senza profilo locale resta bloccata prima delle attivazioni. Non è una release qualificata per il gauge o per le due schede non identificate.

## Stato sintetico corrente — V0.13

| Modulo | Implementazione disponibile | Limite attuale |
|---|---|---|
| Owner/main | `ag_application` compone discovery/config/runtime/journal/UI/RPC/boot; tre profili compilati | Default bloccato; banco volatile; nativo richiede profilo locale qualificato |
| Configurazione/storage | Commit/readback, provisioning blank-only separato, journal e adapter partizioni reali | Nessun erase/default automatico; sizing V1 e fault fisici da qualificare |
| Diagnostica | 53 decoder scalari e discovery per ECU/page su quattro varianti CAN | Non copre tutti i PID standard; nessuna compatibilità ECU fisica dimostrata |
| TWAI | Guardie fresche, completion TX, ISR queue, quiesce/suspend/resume solo da stato sano | Pin e profilo locali; fault/ambiguità terminali, nessun recupero bus-off automatico |
| Runtime | Polling/log autonomi, rate budget globale per PID conservato fra configurazioni | Prestazioni e tempi IO fisici non misurati; logging degradato opt-in esplicito |
| UI gauge | Scene, tap e renderer RGB565 circolare con buffer per righe | Power/protocollo pannello, touch, ALS e IGN richiedono port qualificato |
| BLE/OTA | Owner sul task host, endpoint RPC condiviso, route OTA esclusiva, trust/installer/NVS boot legati | Profilo assente rifiuta prima di attivare hardware; chiavi produzione non fornite |
| Reset | Record NVS canonico, intent/fasi durabili e recovery locale dopo interruzione | Cancellazione credenziali persistenti/RAM/controller richiede hook qualificato; audit dedicato |
| App | Recovery archivio→inbox e flusso import/verifica/upload/READY/select esplicito | Provider nativo/trust assenti di default; nessun cloud/radio live qualificato |

[Applicazione](../docs/software/APPLICATION.md), [manutenzione](../docs/software/MAINTENANCE.md),
[renderer](../docs/software/GAUGE_RENDERER.md), [consegna](../docs/software/SOFTWARE_DELIVERY.md).
Le sezioni FW-02… e V0.12 sotto conservano la cronologia; i vecchi conteggi o
main inerti riferiti a quegli incrementi non descrivono il target unificato V0.13.

## Compilazione e test host

Richiede CMake ≥3.20, compilatore C11 e Python 3. Nessun pacchetto Python esterno. Clang/GCC attivano AddressSanitizer e UndefinedBehaviorSanitizer per impostazione predefinita.

```sh
cmake -S firmware -B firmware/build-host -DCMAKE_BUILD_TYPE=Debug
cmake --build firmware/build-host
ctest --test-dir firmware/build-host --output-on-failure
firmware/build-host/gauge_core_tests
firmware/build-host/gauge_diagnostics_tests
firmware/build-host/gauge_connection_tests
firmware/build-host/gauge_history_tests
firmware/build-host/gauge_history_fixture_cli
python3 firmware/tools/check_history_interop.py --executable firmware/build-host/gauge_history_fixture_cli
```

Da una directory già interna a `firmware/`, usare `-S . -B build-host`.

La suite corrente contiene23CTest. I conteggi e gruppi descritti di seguito sono quelli storici del primo nucleo: `gauge_core_tests` copre otto gruppi: fixture condivise, configurazione/rollback RAM, guardia CAN, Service 01, rate/stati indisponibili, overflow/limiti coda, clock/sessione e HAL bloccato. `gauge_diagnostics_tests` copre dieci gruppi: 13 vettori decoder, risposte malformate, bitmap/sorgenti ECU, profili e SF, FF/CF e wrap sequenza, errori e timeout, 10000 ingressi casuali bounded, scheduling/timeout, round-robin/clock e revoca di ogni guardia tra prepare e dispatch. `gauge_connection_tests` copre otto gruppi: A→busy B→B→A senza cancellazione, auth mancante, lease/boot/counter, qualificazione fisica, 18 punti di interruzione prima/dopo I/O, restore corrotto/mancante/ripetuto, USB persa/intent non confermato e journal/counter estremi. `gauge_history_tests` copre cinque gruppi e 12 pagine golden; `gauge_history_interop` confronta quelle pagine con Python e due archivi SQLite distinti.

Il generatore `tools/generate_fixture.py` legge direttamente i JSON in `../shared/fixtures/` e genera un header solo nella directory di build. Al momento 19 dei 22 casi comuni sono rappresentabili in strutture C tipizzate; i 3 casi con booleani/frazioni richiedono il futuro parser JSON e **non sono dichiarati testati sul device**. I test effettuano anche controllo capability, limite 3×5, rifiuto di coordinate estreme, conservazione configurazione invalida e verifica delle richieste per tutti i 256 valori PID.

Gli assert dei test restano attivi anche in build Release (`-UNDEBUG`). Gli assert nello script di controllo del repository non sostituiscono questa suite né prove su hardware.

## Interfacce e limiti di memoria

`include/ag_core.h` usa oggetti C interni, non strutture da spedire direttamente come byte: padding, endianess e dimensione `size_t` dipendono dalla piattaforma. Il futuro decoder JSON deve verificare tipi e range prima di convertire; `true`, frazioni e overflow non vanno normalizzati silenziosamente. Luminosità `manual_percent` e rettangoli sono gli stessi valori interi del contratto, senza conversioni float.

La maschera capability è distinta dall'allowlist diagnostica. Quest'ultima comprende inizialmente PID `04,05,0C,0D,0F,10,11`; le pagine di discovery sono `00,20,40,60,80,A0,C0,E0`. La presenza di un PID nella maschera non autorizza servizi diversi da 01 o PID fuori dall'allowlist. Una capability deve rappresentare una misura realmente utilizzabile, non il solo numero presente nel bitmap ECU. I sette decoder iniziali sono coerenti con il catalogo simulato dell'app; non soddisfano ancora il requisito di tutti i PID Service 01 standard.

La coda ha 64 record e al massimo 15 coppie ECU/PID per sessione, con memoria costante, nessun heap e nessuna I/O bloccante. Una sedicesima serie restituisce `AG_LOG_SERIES_FULL`: occorre gestirlo esplicitamente nel futuro servizio, non eliminare tacitamente una serie. Payload raw massimo **8 byte**, ipotesi per questa iterazione; payload più lunghi sono **rifiutati**, mai troncati. Per il supporto completo servono formato variabile o record estesi, decoder e nuovo sizing. Gli stati unavailable/timeout/stale/unsupported/error richiedono payload vuoto; valid richiede payload non vuoto. Non viene riciclato un valore precedente come attuale.

All'overflow viene sovrascritto il record più vecchio e si aggiornano `dropped_records`, `first_dropped_sequence`, `last_dropped_sequence`. L'intervallo è l'inviluppo delle perdite della sessione, non la dichiarazione che ogni sequenza intermedia sia persa se il consumer ha svuotato parte della coda. La numerazione non torna a zero a un wrap: il limite numerico blocca nuove append. Chiamare da un solo task proprietario, fuori da ISR/callback BLE; il driver futuro deve instradare i dati a quel task.

I 2 MiB di flash approvati in DEC-006 sono condizionati al sizing OTA. Qui non è creata alcuna tabella partizioni e non viene sottratto spazio alla recovery. Questa coda RAM non garantisce ore di storico, sopravvivenza a reset/crank, retention o usura flash.

Le funzioni di inizializzazione queue/clock richiedono che `session_id` e `configured` siano oggetti esterni alla struttura di destinazione: non è supportato passare puntatori ai suoi campi prima del `memset`. I chiamanti attuali rispettano questa precondizione.

## FW-02 — diagnostica su frame simulati

`include/ag_diagnostics.h` separa parsing, reassembly e scheduling dal backend hardware. Non contiene un'API di invio frame CAN raw.

| PID hex | Payload dati | Conversione implementata | Unità |
|---|---|---|---|
| 04 | A | A × 100 / 255 | % carico |
| 05 | A | A − 40 | °C refrigerante |
| 0C | A,B | (256 × A + B) / 4 | rpm |
| 0D | A | A | km/h |
| 0F | A | A − 40 | °C aria aspirata |
| 10 | A,B | (256 × A + B) / 100 | g/s MAF |
| 11 | A | A × 100 / 255 | % farfalla |

I decoder ricevono l'application payload ISO-TP già privato del padding CAN. Richiedono risposta positiva `41 PID` e lunghezza esatta per quel PID; servizio/PID errati, dati corti o extra non producono un valore. Il raw conservato contiene solo A/B, non i byte `41 PID`. Una risposta negativa `7F 01 NRC` è segnalata come errore e conserva NRC separatamente, senza valore. La policy iniziale chiude la transazione anche per response-pending: gestione estesa NRC e retry non implementati. Non vengono dedotte unità/scale per PID ignoti.

`ag_obd_capabilities_apply` valida `41 page A B C D`, allinea le pagine a multipli di 0x20 e aggiorna solo i relativi bit della stessa sorgente ECU. Il bit più significativo di A corrisponde a page+1, l'ultimo di D a page+32. L'ultimo bit della pagina E0 supera lo spazio PID a 8 bit ed è ignorato senza wrap su zero. `ag_obd_usable_pids` interseca il bitmap pubblicizzato con i sette decoder implementati. Il builder discovery e il parser bitmap esistono, ma non è ancora presente un orchestratore di discovery su bus: la suite fornisce capability simulate per ECU.

Reassembly: una sola risposta per contesto, **256 byte massimi**, frame CAN classici di 1–8 byte, normal addressing. CAN ID standard 11 bit ed extended 29 bit sono supportati come dati di profilo; **29-bit CAN ID non significa supporto dell'extended/mixed ISO-TP addressing**, che è assente. Il timeout tra frame è un parametro obbligatorio, non un valore normativo inventato. Sono verificati dimensione annunciata, SF/FF/CF, sequenza a 4 bit con wrap, CF troppo corte, source ID/format e regressione temporale. Frame di altre ECU non concatenano dati né prolungano il timeout. CAN FD, RTR, frame Flow Control in ingresso e FF a lunghezza estesa 32 bit non sono supportati. Il padding finale viene ignorato; valore del padding non validato. STmin viene verificato soltanto nella modalità Flow Control FW-06.

La modalità RX passiva originaria non emette Flow Control; FW-06 aggiunge una modalità opzionale e handoff host di FC con guardia TX. Anche con tale modalità, hardware, trasmissione e conferma TX reali restano assenti: non è dimostrata conformità ISO 15765-2/4 o compatibilità Polo.

Lo scheduler usa massimo 15 job univoci ECU/PID e al più una transazione complessiva, preparata oppure attiva. Tutti i job appartengono a un solo bitrate/formato per istanza: cambiare variante richiede nuova inizializzazione. Periodo, timeout risposta e soglia stale sono parametri positivi del chiamante; i valori 200/100/50 ms nelle fixture sono numeri di test, non requisiti di prodotto. Dopo completamento o timeout attende almeno un periodo prima di riprovare quel job; round-robin favorisce i job pronti senza promettere 5 Hz.

`prepare` non autorizza un invio successivo: `dispatch` rivaluta tutte le guardie, cancella il lavoro se IGN è diventato ignoto/OFF, USB è l'unica fonte, standby è richiesto, hardware/rail non sono qualificati, bus-off è presente o la sessione diagnostica è inattiva. Anche la ricezione e il tick rivalutano lo stato: nessun payload pronto o valore valido sopravvive come dato attuale a un blocco della sessione. La regressione del clock blocca lo scheduler fino a nuova inizializzazione; un timeout rende la misura TIMEOUT, un valore troppo vecchio STALE, una misura mai ricevuta o una sessione bloccata UNAVAILABLE. In ogni caso indisponibile `has_value=false`, `value=NaN` e raw vuoto.

Il dispatch restituisce soltanto un handoff host `profile + [01,pid]`. Un futuro backend dovrà rivalutare la guardia nella stessa sezione critica dell'effettivo accodamento TX, svuotare richieste pendenti al cambio stato e gestire l'esito del driver: la semplice costruzione di un payload non è un'autorizzazione persistente. Lo skeleton `app_main` non usa lo scheduler e continua a non inizializzare TWAI o transceiver.

Fonti consultate il 24/10/2026: implementazioni primarie [python-OBD comandi](https://raw.githubusercontent.com/brendan-w/python-OBD/master/obd/commands.py), [decoder](https://raw.githubusercontent.com/brendan-w/python-OBD/master/obd/decoders.py) e [unità/scaling](https://raw.githubusercontent.com/brendan-w/python-OBD/master/obd/UnitsAndScaling.py), [documentazione ISO-TP del kernel Linux](https://docs.kernel.org/networking/iso15765-2.html) e [implementazione Zephyr](https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/subsys/canbus/isotp/isotp.c). Le formule sono state confrontate con quelle implementazioni pubbliche; il testo completo SAE/ISO non è stato acquisito e non si dichiara validazione normativa. Il codice del progetto è scritto qui senza incorporare sorgenti o dipendenze di quei progetti.

### Riscontri della revisione separata del core

Un secondo subagente ha letto i quattro moduli C e l'header, senza modifiche: non ha trovato bypass delle guardie o accessi fuori limite nei percorsi esaminati. È una review limitata, non una verifica formale. Sono da conservare due vincoli API: il futuro HAL deve ricontrollare il consenso CAN immediatamente prima della trasmissione, anche per una richiesta costruita o accodata quando il consenso era valido; gli argomenti di `ag_log_queue_init` e `ag_session_clock_init` non devono puntare dentro l'oggetto da inizializzare, perché l'inizializzazione azzera prima la destinazione. I chiamanti attuali rispettano quest'ultimo vincolo. Nessun HAL di invio reale è attivo.

## Skeleton ESP-IDF

Directory `idf/`, target di sviluppo ESP32-S3. Lo skeleton è ora cross-compilato con **ESP-IDF ufficiale v6.1/GCC15.2.0**, SDK/toolchain in cache esterna e nessun flash. App `.bin`170624byte, immagine170512byte prima del padding. Versioni, hash, memoria e confini sono in [DEVICE_BUILD.md](../docs/firmware/DEVICE_BUILD.md) e `device-build-report.json`. Questa build verifica compilazione/link del target; non esegue l'applicazione sul dispositivo né congela la release SDK di produzione.

Con la cache dedicata descritta nel report, il comando di sola build è:

```sh
sh firmware/tools/build_esp32s3.sh
```

Il comando è stato eseguito con successo. Non effettuare flash o collegamento alla vettura da questo scaffold. `app_main` genera un identificatore di sessione, inizializza il clock e legge l'HAL, che restituisce potenza/IGN ignoti e non permette TX. Poi resta in un loop di sviluppo; non configura GPIO né chiama driver bus/radio, non scrive flash, non tocca eFuse o chiavi. Il linker rimuove buona parte del core inutilizzato: questo binario piccolo non prova il dimensionamento di firmware completo, asset, OTA o buffer flash.

`esp_fill_random` viene usata soltanto per un identificatore di boot, non come chiave o token di autenticazione. Un ID nullo blocca l'inizializzazione; l'unicità e l'entropia richieste per il protocollo persistente devono essere verificate nella futura implementazione. Il tempo monotono da `esp_timer_get_time` riparte dopo deep sleep: ogni nuovo boot necessita una sessione distinta.

Nessun GPIO pannello viene dedotto dallo schema A0. I pin CAN/power dello schema sono ancora ipotesi; il backend HAL dovrà essere implementato solo dopo verifica delle alimentazioni parziali e dei default elettrici. TCAN1043A con EN=0/nSTB=0 è **Standby con INH ancora attivo**, non Sleep; serve la sequenza qualificata della tabella modi e dei flag WAKERQ. `standby_requested` in software significa blocco del polling, non prova che rail e transceiver siano spenti. Il loop corrente non raggiunge né dimostra il target 100 µA entro 5 minuti.

BLE/autenticazione/antireplay e OTA rimangono bloccati; nessun GATT utilizzabile è pubblicizzato. Non esiste proprietà esclusiva permanente del gauge: DEC-011 ammette più client in sequenza con una sola connessione attiva. Secure boot, rollback e recovery richiedono implementazione e prove prima di qualsiasi provisioning irreversibile.

## FW-03 — una connessione e reset fisico separato

Il contratto aggiornato è [CONNECTION_POLICY_V0.1.md](../docs/protocols/CONNECTION_POLICY_V0.1.md), basato su DEC-011. `include/ag_connection.h` e `src/connection.c` non contengono un registro owner, un trasferimento di proprietà o una lista con N massimo di telefoni. Conservano solo il client della connessione corrente. La dimensione interna del suo identificatore opaco è 16 byte, ipotesi tecnica da mappare nel futuro adapter, non un limite al numero N di client utilizzabili nel tempo. L'effettiva capacità di bond del controller/SDK richiederà un bilancio di risorse e non viene presentata come nuova limitazione di prodotto.

`ag_connection_open` accetta un client solo in `IDLE`; un altro tentativo risponde `BUSY` senza preemption. Ogni nuova connessione è `UNVERIFIED`, anche per un telefono usato in precedenza. Disconnettere A e collegare B o nuovamente A non invoca callback di persistenza o erase e conserva configurazione/log/bond. I cursori storico sono locali a ciascun telefono: l'ACK di A non autorizza la cancellazione di record ancora disponibili per B; il futuro ring rimane bounded con overwrite DEC-006. Questo incremento non implementa produttore flash o reclaim globale.

Il lease comprende boot ID 128 bit e contatore 64 bit crescente. Callback tardivi di close/auth dal vecchio lease non toccano la nuova sessione. Il contatore non fa wrap; esaurito, nuove connessioni sono rifiutate fino a un nuovo boot con identificatore distinto. L'ID di boot deve essere nuovo a ogni inizializzazione, inclusi deep sleep/riavvio: la correlazione del lease non è crittografia né prova dell'identità del client. Il valore non va trattato come segreto.

`ag_connection_authenticate` invoca solo un verificatore esterno fidato per il lease corrente e ricontrolla il lease dopo il callback. Senza verificatore restituisce `AUTH_UNAVAILABLE`; con quello HAL attuale ritorna rifiuto. Nessun booleano proveniente da BLE viene promosso a autenticazione. I test modellano una verifica esterna legata a client+boot+contatore, senza implementare una challenge o chiavi proprie. `mutations_allowed` resta false fino a tale verifica ed è sempre false durante manutenzione. Questa funzione è una guardia del core, non un'autorizzazione effettiva a config/OTA: sessione cifrata, antireplay e controllo degli opcode dovranno essere implementati e testati nell'adapter reale.

Le operazioni del core, gli eventi radio e il reset devono essere serializzati da un unico task o lock. Il codice non è thread-safe e i callback I/O/verifica non devono rientrare nella macchina a stati. Una verifica concorrente non è dimostrata dai test host. Il futuro boot adapter deve leggere e verificare il journal prima di pubblicizzare o accettare BLE; `ag_connection_init` parte infatti bloccata, e solo un restore iniziale valido `CLEAN` abilita lo slot.

Il reset di manutenzione richiede due chiamate locali qualificate: `arm` e `confirm`. Entrambe richiedono origine `LOCAL_PHYSICAL`, USB valida e qualificazione fisica fornita dal futuro HAL; `confirm` richiede inoltre consenso locale esplicito. USB inserita da sola, evento BLE e gesto non qualificato non armano né cancellano. Durata, gesto, debounce e protezione da attivazioni accidentali non sono fissati qui. I campi dell'evento sono metadati di un adapter fidato, non un nuovo comando BLE con cui il remoto possa dichiararsi fisico.

| Fase durevole | Operazione successiva ammessa |
|---|---|
| CLEAN | Solo arm locale: invalida la sessione e scrive ARMED |
| ARMED | Solo conferma locale: scrive INTENT, ancora nessuna cancellazione |
| INTENT | Cancella credenziali/bond/cache locali, poi registra il completamento |
| CREDENTIALS_ERASED | Cancella configurazione locale, poi registra il completamento |
| CONFIG_ERASED | Cancella log locali, poi registra il completamento |
| LOGS_ERASED | Scrive il commit CLEAN finale |

Durante ARMED, IN_PROGRESS o FAILED connessioni e mutazioni restano bloccate. La sessione precedente viene invalidata all'arm, prima di qualsiasi erase. Il callback `erase_credentials` deve cancellare **anche cache RAM di bond, chiavi e verifiche** oltre alle copie persistenti: mantenere il puntatore al verifier non autorizza a continuare a fidarsi di credenziali cache cancellate. Non esiste una callback di cancellazione cloud. Solo dopo tutti gli erase confermati e il commit finale riuscito lo slot torna disponibile, senza owner unico e con nuova autenticazione obbligatoria. I lease precedenti restano invalidi anche se tutto avviene nello stesso boot.

Un passo compie al massimo un erase e la relativa scrittura di journal; le implementazioni reali dovranno essere bounded e coordinate con la messa in quiete dei worker. Un errore I/O o perdita USB rende il manager FAILED e mantiene il blocco. Ripristinando uno snapshot incompleto al reboot non si riabilitano vecchi bond: il sistema resta FAILED finché una nuova qualificazione/conferma locale consente di completare i passi rimanenti. Erase e scrittura devono essere idempotenti, perché uno spegnimento dopo erase ma prima del progresso persistito può far ripetere lo stesso erase. Snapshot mancante, CRC errato o restore fallito bloccano anche il reset automatico; non vengono reinterpretati come stato di fabbrica pulito. Un secondo restore sulla stessa inizializzazione della connessione viene rifiutato.

Il journal è un modello C con versione, generazione di reset, fase, maschera completamenti e CRC-32/ISO-HDLC sui campi canonici (non sul padding della struttura). Il CRC serve a rilevare corruzioni accidentali; **non autentica il journal né impedisce rollback ostili**. Due fixture CRC indipendenti provengono da `binascii.crc32` di Python, e le prove simulano interruzioni prima/dopo ognuna delle nove I/O. L'algoritmo e il polinomio sono quelli pubblici di [RFC 1952, appendice CRC](https://www.rfc-editor.org/rfc/rfc1952). Non sono introdotte primitive crittografiche artigianali.

Il callback di persistenza può rispondere successo solo quando il record è durevole e verificato. Non viene eseguito erase prima che INTENT abbia ricevuto tale successo. Se manca persino il primo ARMED durevole, il reset non è ancora iniziato e nessun dato è stato cancellato: al reboot un journal precedente CLEAN verificato può ancora descrivere la configurazione preesistente, ma il vecchio lease non è valido e l'autenticazione di sessione riparte da zero. Se l'esito di scrittura è ambiguo/corrotto, il futuro adapter deve segnalarlo come restore fallito e restare bloccato. Dopo il commit finale durevole tutti gli erase sono invece già completati; una perdita del solo ACK finale può essere recuperata leggendo CLEAN. Queste differenze sono coperte esplicitamente dai test, senza fingere che uno stato RAM sia persistente.

Non è definito un layout flash, doppio journal, protezione da rollback o prova di atomicità sul componente reale. `app_main` resta inerte; i nuovi HAL verify/erase/journal rispondono rifiuto o `NOT_IMPLEMENTED`. Nessun dato esistente, bond reale, eFuse o chiave è cancellato. Il reset non autorizza CAN e non dimostra standby o recupero elettrico.

## FW-04 — produttore pagine storico C

`include/ag_history.h` e `src/history.c` aggiungono una sorgente di storico fixed-capacity, distinta da `ag_log_queue_t`: leggere o confermare una pagina **non chiama `ag_log_queue_pop` e non consuma lo storico**. Il modello tiene fino a 64 record, una sola sessione/boot e nessuna tabella dei client. La capacità 1–64 è un'ipotesi di test, non la dimensione finale della partizione. Ogni client passa il proprio cursore; ACK di A non elimina i record disponibili per B. La sovrascrittura riguarda soltanto i record più vecchi del ring, secondo DEC-006.

`ag_history_export` produce una copia cronologica senza puntatori al ring. Modificare la copia o il JSON restituito non modifica la sorgente. `ag_history_restore` verifica capacità, stati/raw, ordinamento e contiguità sequenze, monotonia dei timestamp e coerenza del contatore terminale; uno snapshot non valido disabilita la destinazione invece di servire una precedente sessione. Queste sono verifiche strutturali: checksum, journal flash e provenienza dei metadati persistenti restano responsabilità del futuro backend.

`first_sequence` consente recovery/test da metadati fidati, anche oltre 2^53; non è un parametro da accettare dal telefono. La sequenza UINT64_MAX può essere registrata una volta; un flag esplicito `exhausted` impedisce il wrap successivo, mantenendo comunque leggibili e confermabili gli ultimi record. I timestamp sono uint64 monotoni, mai date assolute o double. Un nuovo boot usa una sessione distinta; il cursore richiesto deve riferirsi espressamente a quella sessione. L'eventuale selezione di sessioni storiche su una flash reale non è implementata da questa singola istanza RAM.

`ag_history_make_page` serializza JSON deterministico con `session_id`, `records`, `lost`; sequenza e tempo sono stringhe decimali canoniche, status esplicito e raw esadecimale minuscolo. Il modello di storico ammette fino a 64 byte raw come il contratto condiviso, mentre il logger RAM originario resta limitato a 8: non sono ampliate implicitamente le capability del decoder o del logger device. Ogni pagina ha fino a 16 record e al massimo 4096 byte **escluso il terminatore NUL**. Il buffer consigliato è quindi 4097 byte.

L'intervallo di perdita viene calcolato dalla prima sequenza ancora conservata e ritagliato al cursore della richiesta: non viene copiato l'inviluppo globale delle perdite RAM. Per il ring contiguo modellato esiste al massimo un intervallo perso iniziale; buchi interni allo snapshot vengono rifiutati, non colmati con perdite inventate. La futura pipeline dovrà modellare separatamente eventuali campioni persi prima di arrivare allo storico.

La serializzazione avviene in staging locale; un buffer di destinazione troppo corto produce `OUTPUT_SHORT`, output vuoto se scrivibile e metadati azzerati, senza pagina parziale o avanzamento di cursore. `info.written` esclude NUL e `info.through_sequence` descrive solo la copertura della pagina: **non è un ACK di import**. Il client può confermare l'import soltanto dopo il proprio commit. `ag_history_acknowledge` è const/stateless e controlla sessione e limite superiore; non elimina record, non modifica il ring e non conserva cursori di N telefoni.

Source e snapshot occupano rispettivamente 6208 e 6200 byte sull'host; `make_page` usa inoltre staging da 4097 byte e piccoli buffer di record/suffisso. Il dimensionamento del task e lo stack reale ESP32-S3 devono essere verificati prima dell'integrazione. Export/append/restore richiedono un task proprietario o un lock; la trasmissione può lavorare sulla copia, ma il futuro adapter deve verificare il lease BLE corrente e invalidare pagine pendenti al reset. Nessuna operazione di questo modulo autorizza radio o autenticazione.

Il generatore `tools/generate_history_fixture.py` legge `../shared/fixtures/pagination.cases.json` e produce un header solo nella build directory. Le sei fixture condivise coprono ring sovrascritto, raw massimo, timeout, precisione oltre 2^53, sequenza terminale uint64 e sorgente vuota. `gauge_history_tests` verifica 12 pagine JSON esatte, ACK senza mutazioni, snapshot indipendenti, restore invalido, budget, output corto, sessione e overflow. Il caso con raw64 e sequenza/timestamp a20cifre riduce la prima pagina a15record/4046byte, lasciando i cinque successivi alla richiesta seguente.

Harness host per l'integrazione:

```sh
firmware/build-host/gauge_history_fixture_cli [case_name]
python3 firmware/tools/check_history_interop.py --executable firmware/build-host/gauge_history_fixture_cli
```

Senza argomento il CLI emette una riga JSON per ogni lettura delle sei fixture: `{"case":"overwritten_ring","read_index":0,"page":{...}}`. Un nome caso sconosciuto termina con codice2. I record vengono costruiti dal C con le fixture, poi serializzati dal produttore: il CLI non ristampa il JSON golden e non contiene un parser JSON device. Il wrapper confronta C/golden/Python, controlla riproducibilità e budget, importa le pagine in due archivi SQLite indipendenti e verifica che l'ACK di A preservi la stessa disponibilità per B.

Tutto questo resta un modello host con snapshot che rappresentano record già conservati. Non esistono driver flash, partizione2MiB, erase di produzione, recovery fisica o BLE reale; `app_main` non usa il produttore e resta inerte. Le scelte di struttura non anticipano il sizing degli slot OTA né una promessa di retention dopo un reset elettrico.

## FW-05 — persistenza configurazione simulata

`ag_config_store.h/config_store.c` separano il protocollo di commit da `ag_flash_sim.h/flash_sim.c`, backend RAM con fault injection. I callback read/program/erase operano su offset e lunghezze bounded; una lettura o programmazione incompleta restituisce il numero effettivo di byte, erase restituisce un esito. Non ci sono heap, puntatori persistenti, serializzazione di struct o parser JSON. Il modello usa **due settori da 4096 byte**: è un formato di sviluppo, non una partizione scelta per il gauge. `app_main` non lo richiama e nessun adapter flash reale è implementato.

Tutti gli interi persistenti sono little endian espliciti. Header di 32 byte:

| Offset | Campo |
|---|---|
| 0–3 | Magic ASCII `AGCF` |
| 4–5 | Versione formato uint16 = 1 |
| 6–7 | Lunghezza payload uint16, massimo 167 |
| 8–15 | Generazione uint64 positiva, mai wrap |
| 16–19 | Schema configurazione uint32 = 1 |
| 20–23 | Revisione uint32, 1…INT32_MAX |
| 24–27 | CRC32 ISO-HDLC |
| 28–31 | Riservato, zero |

Il CRC copre header 0…23, header 28…31 e payload; non include il campo CRC. Il payload contiene schema/revisione uint32, brightness mode/manual percent/dashboard count uint8; ogni dashboard contiene ID/count uint8 e ogni widget ID/PID uint8 più x/y/width/height uint16. Ordine dashboard/widget preservato, nessun padding. Il record è lungo al massimo 199 byte; tutti i byte successivi devono essere `0xff` fino al marker ASCII `COM1` ai byte 4092…4095. Il marker viene programmato **per ultimo**, dopo verifica del corpo completo.

`open/restore` leggono e controllano interamente entrambi i settori: marker, magic, formato, lunghezze, CRC, riservati/padding e validazione strutturale della configurazione. Scelgono la generazione integra più recente; una copia corrotta o incompleta consente soltanto fallback diagnostico READ_ONLY alla precedente integra, senza dichiararla corrente/operativa. Generazioni uguali con contenuto diverso, oppure generazione maggiore con revisione non crescente, producono `CONFLICT` e nessuna configurazione applicabile. Copie identiche sono ammesse. Entrambi gli slot invalidi producono `NO_VALID_CONFIG`: nessun default, bootstrap o overwrite automatico. Il seed iniziale nei test è esplicito e non è un endpoint di provisioning device.

La selezione per integrità è indipendente dalle capability del veicolo corrente. Lo snapshot integro più recente resta selezionato anche se usa un PID non disponibile: `CAPABILITY_MISMATCH`, `ready=true`, `compatible=false`, revisione e generazione conservate. `get` restituisce una **copia diagnostica** per interfaccia/riparazione; il chiamante deve controllare `compatible` prima di applicarla al veicolo. Non si torna silenziosamente a una configurazione più vecchia. Un aggiornamento correttivo deve usare la nuova revisione attesa e capability attuali; la policy completa di cambio veicolo resta aperta. Se un settore ha un errore I/O oppure è non-erased ma invalido per marker/header/CRC/padding/schema, l'altro integro è disponibile come `READ_ONLY`, con flag `compatible` separato e commit vietati fino a letture sane: la revisione non letta potrebbe essere più recente. In questo stato `get` è esclusivamente diagnostico e può esporre una revisione più vecchia; prima di qualsiasi uso operativo occorre verificare esito di restore, `writable` e `compatible`. Un settore completamente erased è invece una copia inattiva sana. Non esiste riparazione/erase automatico: un’interruzione parziale può richiedere manutenzione esplicita prima di tornare scrivibile e questo modello non qualifica autonomia V1 dopo crank.

`commit(expected_revision,candidate)` rilegge entrambi gli slot prima di scrivere, richiede revisione attesa esatta e nuova revisione +1 senza overflow, poi valida la candidata sulle capability attuali. Cancella solo lo slot inattivo, verifica tutti i byte cancellati, programma e rilegge corpo/padding, programma marker, rilegge e verifica il record committato. Lo slot attivo precedente non viene cancellato durante questa transazione. La perdita di alimentazione dopo il marker durevole ma prima dell'ACK può rendere visibile al reboot la nuova revisione benché il chiamante abbia ricevuto errore: prima di ritentare deve leggere lo stato e usare expected revision, senza ripetere ciecamente una mutazione.

Il simulatore applica la regola program 1→0 e permette taglio dopo ogni byte di ciascuna operazione; reboot elimina il guasto mantenendo i byte. I test coprono **24653 tagli a confini di byte in otto fasi**, bitflip in ciascuno dei 4096 byte dello slot nuovo, letture corte, erase fallito o adapter che mente sull'erase, corruzione in programmazione, overflow, conflitti, output corto, capability cambiate e aggiornamento correttivo. Un golden binario generato indipendentemente con Python `struct`/`binascii.crc32` deriva dalla fixture JSON condivisa; i 19 casi rappresentabili in C sono verificati anche in encode/restore. Bool/frazioni restano test del futuro parser JSON.

Sull’host lo store occupa 520 byte e il simulatore completo 8480 byte (include gli 8192 byte dei settori e la strumentazione di test). Il codice usa buffer locali da 4096 byte per la lettura e staging massimo 199 byte, più due descrittori di slot e copie configurazione: `restore` conserva inoltre `slot_info[2]` e `commit` mantiene record e copie della configurazione durante le chiamate annidate. Prima dell’uso runtime serve un task con stack esplicitamente dimensionato e misurato sul target; lo stack predefinito di `app_main` non è qualificato per queste operazioni. Tutti gli accessi richiedono un unico task o lock e callback non rientranti; i test host non dimostrano concorrenza. CRC non autentica né impedisce rollback deliberati. Il modello non dimostra erase/program atomicity del componente reale, wear leveling, cifratura, migrazioni, provisioning iniziale o recupero di schema futuro. Il reset-config erase hook non è collegato a questo simulatore e resta inerte sul device.

## FW-06 — Flow Control host per risposte Service 01

Il reassembly classico offre `ag_isotp_enable_flow_control(BS,STmin)` da stato idle. Lo scheduler la abilita con BS8/STmin0 come **parametri di simulazione**, non taratura della vettura. Dopo una FF valida il ricevitore richiede CTS; dopo ogni blocco BS di CF ne richiede un altro, salvo messaggio completato. BS0 autorizza tutte le CF residue. Una FF con lunghezza oltre 256 byte prepara soltanto overflow (`0x32`), mai CTS o accumulo parziale. Non sono implementati WAIT, trasmissione di PDU multi-frame o richieste con servizi arbitrari: le richieste applicative continuano a essere esclusivamente Service01 e single frame.

`ag_isotp_fc_prepare/dispatch` e i wrapper scheduler separano preparazione e handoff. Entrambi verificano `ag_can_tx_allowed`; il dispatch ricontrolla IGN qualificato, alimentazione OBD, rail/hardware, standby, sessione e bus-off. Se la guardia cambia, cancellano lo stato e restituiscono frame vuoto, senza possibilità di riprendere la vecchia preparazione dopo riabilitazione. Il frame FC usa l’ID fisico request del profilo e formato11/29bit, DLC8, PCI0x30/0x32, BS/STmin e padding zero. Gli indirizzi funzionali OBD noti 0x7DF/0x18DB33F1 vengono rifiutati: il chiamante deve fornire la coppia fisica verificata, che il codice non deduce dalla risposta. Restano possibili configurazioni sbagliate ma numericamente valide: nessuna compatibilità veicolo è inferita dal solo ID.

In modalità FC una FF deve iniziare con risposta Service01 positiva0x41 oppure negativa7F01. Ciò non sostituisce il parser PID: un payload lungo non previsto dai sette decoder rimane errore, anche se ricomposto correttamente. Una CF prima del CTS, oltre il confine BS o con sequenza errata interrompe il messaggio. Frame di altre ECU vengono ignorati e non prolungano i timer. Il contatore di sequenza CF fa wrap4bit; i timer uint64 usano differenze senza scadenze sommate e una regressione del clock invalida la transazione.

STmin pubblicizzato/verificato è **0…127ms**; il clock pubblico è in millisecondi. Le codifiche microsecondi 0xF1…0xF9 e quelle riservate vengono rifiutate esplicitamente. Il limite si verifica tra due CF, anche attraverso confini di blocco, non tra FC e prima CF. Una CF troppo ravvicinata interrompe il reassembly: è una policy rigorosa del modello, non una dichiarazione di comportamento identico agli stack vendor. Attesa del dispatch FC e attesa CF hanno timeout parametrico; il timeout complessivo dello scheduler continua ad applicarsi dal dispatch della richiesta, senza essere rinnovato dai frame.

Il dispatch è solo un **handoff simulato riuscito**, non una conferma di invio CAN. L’adapter futuro dovrà serializzare gli eventi, ricontrollare la guardia immediatamente prima del driver, notificare/gestire fallimento o ritardo dell’invio e qualificare i timer sul completamento effettivo. Nessun callback attiva TWAI, transceiver o radio; `app_main` resta inerte. Non sono supportati extended/mixed addressing, CANFD, lunghezza FF32bit, RX di FC per un sender multiframe o conformità certificata.

`gauge_flow_control_tests` verifica messaggi 256 byte a 11/29 bit su BS0…3/255, STmin e confini BS, wrap sequenze, overflow 257 byte, CTS/overflow con cambio di tutte le nove guardie, preparazioni invalidate, ECU diversa, FF/CF malformate, richiesta di servizio errato, timeout e clock al limite uint64. Verifica inoltre i wrapper scheduler, il rifiuto di un valore PID da un payload ricomposto ma invalido e nessuna trasmissione dopo reset o blocco.

Fonti primarie consultate il 24/10/2026: [documentazione ISO-TP del kernel Linux](https://docs.kernel.org/networking/iso15765-2.html), [manuale API ISO-TP PEAK-System](https://www.peak-system.com/produktcd/Pdf/English/PCAN-ISO-TP-API_UserMan_eng.pdf), [documentazione TWAI Espressif](https://github.com/espressif/esp-idf/blob/v6.1/docs/en/api-reference/peripherals/twai.rst). Queste fonti motivano le opzioni e i limiti; non sostituiscono accesso e verifica del testo normativo ISO15765-2/4 per la release finale.

## FW-07 — ammissione telefoni DEC-014, controller host

`ag_admission.h/admission.c` implementano il flusso locale «Associa telefono» separato da reset, owner e cloud. Il modulo si collega una sola volta a un `ag_connection_t` inizializzato/idle con verifier assente e ne installa un bridge interno; conserva il vincolo di un solo link e il blocco del journal di boot. Più identità possono essere utilizzate in sequenza: riconosciuto/non riconosciuto deriva da un lookup fidato, non dal MAC dichiarato dal telefono. Il core non conserva un registro di N bond, chiavi Bluetooth o cursori globali.

Un client nuovo richiede una finestra aperta da un evento locale qualificato del menu touch. Il booleano `qualified_local_menu` è il confine di un adapter HAL fidato, **non un campo del protocollo BLE**; non deve esistere un opcode remoto per impostarlo. La connessione occupa lo slot in stato unverified, senza letture private, notification, indication o mutazioni. Solo l’azione display SMP del tentativo corrente può richiedere la passkey. Il controller richiede un callback di entropia obbligatorio e usa rejection sampling di uint32 sotto 4294000000 prima del modulo 1000000, con massimo 8 estrazioni: errore o rifiuti esauriti chiudono link/finestra, senza fallback fisso. Il codice copre 000000…999999 e viene mostrato con sei cifre, incluse quelle iniziali zero. Il core non lo stampa, pubblicizza, persiste o confronta con un valore inviato da un comando remoto. Le chiusure del core sono transizioni host: l’adapter futuro deve anche interrompere il link fisico, invalidare code notifiche/comandi, mantenere il limite stack di un link e cancellare il display prima di accettare una nuova connessione. L’adapter futuro dovrà fornire entropia qualificata, consegnare il valore solo allo stack SMP e al display locale, cancellare il display e i propri buffer quando il tentativo termina.

I telefoni riconosciuti possono aprire lo slot libero senza finestra o codice, ma ripartono sempre unverified. `security_changed` legge da un hook stack fidato evidenza legata a identità risolta e lease corrente: link cifrato, MITM autenticato, LE Secure Connections, bond disponibile e chiave 128 bit sono tutti obbligatori. Per un nuovo client è richiesta anche passkey emessa nel tentativo corrente, evidenza stack che proprio tale passkey display sia stata usata dal pairing di quel lease (MITM con altro metodo non basta) e conferma che lo stack abbia conservato un bond utilizzabile; storage pieno/fallimento resta errore, senza eviction automatica o erase della configurazione/log. Soltanto durante questa verifica il bridge interno può autorizzare `ag_connection_authenticate`; chiamare direttamente quest’ultima con un proof remoto non produce autorizzazione. Non è implementata crittografia in questo controller: i test simulano un adapter fidato, non dimostrano autenticazione Bluetooth reale.

`ag_admission_private_allowed` deve essere eseguita immediatamente prima di **qualsiasi** lettura privata, notifica/indicazione, scrittura o comando. Richiede il lease già verificato e rilegge lo stato di sicurezza effettivo; downgrade o hook fallito invalidano il link. Il vecchio `ag_connection_mutations_allowed` da solo non rivaluta lo stato radio ed è insufficiente per l’adapter di produzione. Eventi/accessi/invio devono essere serializzati nello stesso task o lock, senza callback rientranti, per non introdurre una finestra tra controllo e utilizzo. Nessuna funzione di ammissione autorizza CAN o OTA.

Lease boot+contatore respingono callback tardivi di A sulla connessione B. Disconnect, timeout, errore di entropia, regressione del clock o manutenzione cancellano passkey e stato locale; il reset resta una procedura distinta. Un repeat-pairing remoto viene rifiutato senza eliminare bond o sostituire il peer attivo. Un bond perso richiede il normale nuovo abbinamento locale, dopo che il backend fidato lo classifica assente: non viene cancellato automaticamente un record alla richiesta remota. Gestione/UX della cache finita resta nel futuro backend e non limita N come requisito prodotto.

Finestra, timeout di autenticazione, cooldown e tentativi sono parametri **H**. I limiti di risorsa host sono rispettivamente 1…600000 ms,1…600000 ms,1…3600000 ms e1…10 connessioni nuove per finestra; non sono timing di prodotto approvati. I test usano 1000/100/500 ms e 2 tentativi. Una richiesta oltre il budget chiude la finestra e avvia cooldown; client già riconosciuti possono comunque autenticarsi nello slot libero. Una nuova azione locale qualificata può aprire una nuova finestra quando non c’è cooldown. Il clock uint64 usa differenze senza sommare scadenze e una regressione blocca il controller fino alla nuova inizializzazione con boot ID nuovo.

Sul tag SDK v6.1 verificato, l’adapter dovrà impostare un link massimo, display-only, bonding, MITM e SC-only senza fallback legacy/JustWorks/debug keys, poi verificare la negoziazione. `ble_gap_sec_state` espone encrypted/authenticated/bonded/key_size ma **non un flag SC**: non basta copiare quei campi. Il futuro adapter deve provare SC tramite policy SC-only e stato SMP/bond qualificato, verificando key_size 16 byte prima di esporre key_bits 128. L’esempio ufficiale mostra anche logging passkey, modulo casuale e delete-peer su repeat-pairing: tali comportamenti non vengono adottati. I flag Bluetooth nello skeleton restano disabilitati; nessun hook reale o driver è stato attivato.

`gauge_admission_tests` copre A→busyB→B→A, autenticazione non trasferita, callback tardive, passkey con leading zero e rejection sampling, entropia fallita/esaurita, tutti i downgrade/mancanza di evidenza, bypass via proof rifiutato, timeout, cooldown, clock/contatore max, perdita bond, storage pieno, repeat-pairing senza cancellazioni, reset/journal gate e init senza hook. Tutti gli hook sono fixture host esplicite; nessun segreto o passkey viene scritto nell’output dei test.

Fonti primarie: [SMP ESP-IDF v6.1](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-guides/ble/smp.html), [security Kconfig v6.1](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/kconfig-reference.html), [esempio ufficiale NimBLE Security v6.1](https://github.com/espressif/esp-idf/tree/v6.1/examples/bluetooth/ble_get_started/nimble/NimBLE_Security). Verificati anche i sorgenti locali del submodule NimBLE inclusi nel commit SDK fissato nel report; nessun codice vendor copiato come controller di autenticazione.

## Parser JSON C condiviso — verifica di compilazione

Il parser bounded in `../shared/c/ag_config_json.c` è compilato nei target host e ESP32-S3, senza chiamate dal main inerte. Limiti, ABI e frame statici GCC sono documentati in [PARSER_BUILD.md](../docs/firmware/PARSER_BUILD.md): 512 token è un contatore, non un array, e lo stack runtime deve ancora essere misurato e dimensionato. La validazione JSON non concede autorizzazione o commit.

## FW-08 — persistenza storico simulata

Implementazione e limiti in [FLASH_HISTORY_MODEL.md](../docs/firmware/FLASH_HISTORY_MODEL.md): due snapshot streaming, bootstrap esplicito, `begin_session` con SID nuovo conserva i record precedenti, gap per sessione e ACK senza consumo. Recovery degradata resta READ_ONLY; nessuna riparazione automatica. Il modello copia O(capacità) a ogni append e usa metà spazio come copia: non è il backend V1 né una qualifica di endurance, retention o autonomia dopo crank.

## Prossimi incrementi

1. Integrare DEC-011 (N client sequenziali, una connessione) con autenticazione e framing BLE, quindi dispatcher del parser bounded con autorizzazione e commit.
2. Aggiungere adapter di simulazione app–core con ACK di ricezione separato da commit; adapter flash reale e schema di migrazione, partendo dal modello FW-05.
3. Completare discovery, catalogo decoder e adapter CAN/ISO-TP per Flow Control con TX subordinato a hardware/IGN qualificati, budget bus e test di simulatore.
4. Dimensionare immagine/asset/slot OTA prima di abilitare il buffer flash e recupero da interruzione.
5. Chiudere hardware e HAL power/display/ALS, poi prove di banco senza vettura.

Documentazione primaria consultata il 24/10/2026: [ESP Timer release 5.3](https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32s3/api-reference/system/esp_timer.html), [sorgente documentazione Espressif](https://github.com/espressif/esp-idf/blob/master/docs/en/api-reference/system/esp_timer.rst). Nessuna misura hardware o conformità dichiarata.

## Trasferimento OTA bounded FW-13

`include/ota_transfer.h` / `src/ota_transfer.c` implementano codec payload e owner a memoria fissa con lease fidata, offset esatti, deadline/poll e select distinta. Lo studio `ota_study/main/transfer_adapter.c` collega le callback al vero verificatorePSA e installerSDK, senza avviarli dal main. Il CLI `tools/ota_transfer_cli.c` richiede `--trusted-fixture-maintenance` e usa backend comportamentali, senza RSA/flash. Nessuna route BLE/RPC nativa è abilitata. [Contratto](../docs/protocols/OTA_TRANSFER_V0.1.md), [verifica](../docs/verification/VERIFICATION_V0.10.md).

## Endpoint OTA AG FW-14

`ota_framed_endpoint` aggiunge reassembly bounded, staging/step separati, ID sequenziali, ACK correnti e chiusura terminale con cleanup singolo dell’owner. Lo studio `ota_study/main/framed_adapter` collega l’intera catena al verificatore/installer esistenti; il CLI framed richiede ammissione fixture esplicita e non usa flash o crypto reali. Main/bridge NimBLE restano senza route OTA attiva. [Contratto](../docs/protocols/OTA_FRAMED_V0.1.md), [verifica](../docs/verification/VERIFICATION_V0.11.md).

## Nuovi meccanismi V0.12

[Boot health/floor durabile](../docs/protocols/OTA_BOOT_HEALTH_V0.1.md), [discovery/default](../docs/software/DISCOVERY.md), [gauge UI](../docs/software/GAUGE_UI.md), [TWAI](../docs/software/TWAI_PORT.md) e [catalogo sessioni](../docs/protocols/HISTORY_SESSIONS_V0.1.md). Gli studi platform/OTA/BLE compilano API SDK reali con main inerti; non sono un firmware completo pronto al veicolo. Il prossimo lavoro è la composizione in un solo owner/main, non riscrivere questi moduli.
