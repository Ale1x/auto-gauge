# Firmware Auto Gauge — base di sviluppo v0.1

Nucleo C11 compilabile su host e skeleton ESP-IDF per il candidato ESP32-S3. Si può sviluppare e verificare la logica prima che pinout pannello, alimentazioni e rilevamento quadro siano risolti. Questa base non è un firmware funzionante sul gauge: nessun driver CAN, BLE, display, flash log o OTA è attivato.

## Contenuto e requisiti coperti

| Modulo | Implementato su host | Limite attuale |
|---|---|---|
| Configurazione | Schema 1, revisione 1–INT32_MAX, 1–3 dashboard, 1–5 widget, ID unici, rettangoli interi normalizzati 0–10000, luminosità manuale/auto, verifica capability PID | Nessun parser JSON device, persistenza, CRC, migrazione o conferma remota di applicazione |
| Applicazione configurazione | Valida prima di sostituire la configurazione RAM; errore conserva la precedente | Singolo task proprietario; non è commit atomico flash né autorizzazione BLE |
| Guardia CAN | TX bloccato al boot, USB sola, alimentazione ignota, IGN ignoto/OFF, standby richiesto, bus-off, rail non validi o hardware non qualificato | Il produttore reale dell'ingresso `AG_IGN_QUALIFIED_ON` non esiste ancora; traffico CAN e tensione non lo impostano |
| Richieste diagnostiche | Service 01 fisso, allowlist, decoder iniziali e bitmap capability per ECU; profili 11/29 bit e 250/500 kbit/s come dati | Nessun driver TWAI, auto-detection, discovery su veicolo o catalogo completo |
| Trasporto diagnostico | Reassembly ISO-TP RX bounded su frame simulati SF/FF/CF, timeout e sequenza | Solo CAN classico/normal addressing, nessun Flow Control o TX; non è uno stack ISO-TP completo |
| Scheduler | Round-robin host con una transazione attiva, timeout, stato iniziale unavailable/stale distinti, guardia rivalutata al dispatch | Nessuna trasmissione fisica, rilevazione IGN o garanzia 5 Hz |
| Log | Coda RAM fissa, massimo 1 Hz per coppia sorgente ECU/PID, overflow con sequenze perse, stop immediato di nuove append | Nessuna scrittura flash, chunk BLE, deduplica import, resume o power-loss recovery |
| Tempo | Session ID di boot fornito dal chiamante, tempo monotono relativo, regressioni bloccate | Nessun RTC o data assoluta inventata |
| HAL | Interfacce per power/display/log/BLE/OTA | Restituiscono `NOT_IMPLEMENTED` o `BLOCKED`, mai un falso successo |

Le scelte di strutture e capacità sono ipotesi di sviluppo, non nuovi requisiti approvati. Restano normative SPEC_V0.2.md e DECISIONS.md, in particolare DEC-003–008. Il contratto comune è [shared/PROTOCOL_V0.1.md](../shared/PROTOCOL_V0.1.md).

## Compilazione e test host

Richiede CMake ≥3.20, compilatore C11 e Python 3. Nessun pacchetto Python esterno. Clang/GCC attivano AddressSanitizer e UndefinedBehaviorSanitizer per impostazione predefinita.

```sh
cmake -S firmware -B firmware/build-host -DCMAKE_BUILD_TYPE=Debug
cmake --build firmware/build-host
ctest --test-dir firmware/build-host --output-on-failure
firmware/build-host/gauge_core_tests
firmware/build-host/gauge_diagnostics_tests
```

Da una directory già interna a `firmware/`, usare `-S . -B build-host`.

La suite CTest contiene due eseguibili. `gauge_core_tests` copre otto gruppi: fixture condivise, configurazione/rollback RAM, guardia CAN, Service 01, rate/stati indisponibili, overflow/limiti coda, clock/sessione e HAL bloccato. `gauge_diagnostics_tests` copre dieci gruppi: 13 vettori decoder, risposte malformate, bitmap/sorgenti ECU, profili e SF, FF/CF e wrap sequenza, errori e timeout, 10000 ingressi casuali bounded, scheduling/timeout, round-robin/clock e revoca di ogni guardia tra prepare e dispatch.

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

Reassembly: una sola risposta per contesto, **256 byte massimi**, frame CAN classici di 1–8 byte, normal addressing. CAN ID standard 11 bit ed extended 29 bit sono supportati come dati di profilo; **29-bit CAN ID non significa supporto dell'extended/mixed ISO-TP addressing**, che è assente. Il timeout tra frame è un parametro obbligatorio, non un valore normativo inventato. Sono verificati dimensione annunciata, SF/FF/CF, sequenza a 4 bit con wrap, CF troppo corte, source ID/format e regressione temporale. Frame di altre ECU non concatenano dati né prolungano il timeout. CAN FD, RTR, frame Flow Control in ingresso e FF a lunghezza estesa 32 bit non sono supportati. Il padding finale viene ignorato; valore del padding e separazione STmin non sono validati.

**Non è emesso Flow Control.** Un vero mittente ISO-TP normalmente attende FC dopo FF: i test iniettano CF da un peer simulato. Questo componente non è utilizzabile come ricevitore multiframe completo su vettura finché manca FC, con relativo gating TX e timing. Non dichiara conformità ISO 15765-2/4 o compatibilità Polo.

Lo scheduler usa massimo 15 job univoci ECU/PID e al più una transazione complessiva, preparata oppure attiva. Tutti i job appartengono a un solo bitrate/formato per istanza: cambiare variante richiede nuova inizializzazione. Periodo, timeout risposta e soglia stale sono parametri positivi del chiamante; i valori 200/100/50 ms nelle fixture sono numeri di test, non requisiti di prodotto. Dopo completamento o timeout attende almeno un periodo prima di riprovare quel job; round-robin favorisce i job pronti senza promettere 5 Hz.

`prepare` non autorizza un invio successivo: `dispatch` rivaluta tutte le guardie, cancella il lavoro se IGN è diventato ignoto/OFF, USB è l'unica fonte, standby è richiesto, hardware/rail non sono qualificati, bus-off è presente o la sessione diagnostica è inattiva. Anche la ricezione e il tick rivalutano lo stato: nessun payload pronto o valore valido sopravvive come dato attuale a un blocco della sessione. La regressione del clock blocca lo scheduler fino a nuova inizializzazione; un timeout rende la misura TIMEOUT, un valore troppo vecchio STALE, una misura mai ricevuta o una sessione bloccata UNAVAILABLE. In ogni caso indisponibile `has_value=false`, `value=NaN` e raw vuoto.

Il dispatch restituisce soltanto un handoff host `profile + [01,pid]`. Un futuro backend dovrà rivalutare la guardia nella stessa sezione critica dell'effettivo accodamento TX, svuotare richieste pendenti al cambio stato e gestire l'esito del driver: la semplice costruzione di un payload non è un'autorizzazione persistente. Lo skeleton `app_main` non usa lo scheduler e continua a non inizializzare TWAI o transceiver.

Fonti consultate il 24/10/2026: implementazioni primarie [python-OBD comandi](https://raw.githubusercontent.com/brendan-w/python-OBD/master/obd/commands.py), [decoder](https://raw.githubusercontent.com/brendan-w/python-OBD/master/obd/decoders.py) e [unità/scaling](https://raw.githubusercontent.com/brendan-w/python-OBD/master/obd/UnitsAndScaling.py), [documentazione ISO-TP del kernel Linux](https://docs.kernel.org/networking/iso15765-2.html) e [implementazione Zephyr](https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/subsys/canbus/isotp/isotp.c). Le formule sono state confrontate con quelle implementazioni pubbliche; il testo completo SAE/ISO non è stato acquisito e non si dichiara validazione normativa. Il codice del progetto è scritto qui senza incorporare sorgenti o dipendenze di quei progetti.

### Riscontri della revisione separata del core

Un secondo subagente ha letto i quattro moduli C e l'header, senza modifiche: non ha trovato bypass delle guardie o accessi fuori limite nei percorsi esaminati. È una review limitata, non una verifica formale. Sono da conservare due vincoli API: il futuro HAL deve ricontrollare il consenso CAN immediatamente prima della trasmissione, anche per una richiesta costruita o accodata quando il consenso era valido; gli argomenti di `ag_log_queue_init` e `ag_session_clock_init` non devono puntare dentro l'oggetto da inizializzare, perché l'inizializzazione azzera prima la destinazione. I chiamanti attuali rispettano quest'ultimo vincolo. Nessun HAL di invio reale è attivo.

## Skeleton ESP-IDF

Directory `idf/`, target di sviluppo ESP32-S3, baseline documentale ESP-IDF release 5.3. La release SDK di prodotto rimane da fissare. Lo skeleton non è stato compilato per il target: `idf.py` e la relativa toolchain non erano disponibili durante questa iterazione. La build host non verifica gli header/ABI ESP-IDF.

Con un SDK qualificato attivo, il futuro controllo di sola build è:

```sh
cd firmware/idf
idf.py set-target esp32s3
idf.py build
```

Questi comandi non sono stati eseguiti qui. Non effettuare flash o collegamento alla vettura da questo scaffold. `app_main` genera un identificatore di sessione, inizializza il clock e legge l'HAL, che restituisce potenza/IGN ignoti e non permette TX. Poi resta in un loop di sviluppo; non configura GPIO né chiama driver bus/radio, non scrive flash, non tocca eFuse o chiavi.

`esp_fill_random` viene usata soltanto per un identificatore di boot, non come chiave o token di autenticazione. Un ID nullo blocca l'inizializzazione; l'unicità e l'entropia richieste per il protocollo persistente devono essere verificate nella futura implementazione. Il tempo monotono da `esp_timer_get_time` riparte dopo deep sleep: ogni nuovo boot necessita una sessione distinta.

Nessun GPIO pannello viene dedotto dallo schema A0. I pin CAN/power dello schema sono ancora ipotesi; il backend HAL dovrà essere implementato solo dopo verifica delle alimentazioni parziali e dei default elettrici. TCAN1043A con EN=0/nSTB=0 è **Standby con INH ancora attivo**, non Sleep; serve la sequenza qualificata della tabella modi e dei flag WAKERQ. `standby_requested` in software significa blocco del polling, non prova che rail e transceiver siano spenti. Il loop corrente non raggiunge né dimostra il target 100 µA entro 5 minuti.

BLE/ownership/autenticazione/antireplay e OTA rimangono bloccati; nessun GATT utilizzabile è pubblicizzato. Secure boot, rollback e recovery richiedono implementazione e prove prima di qualsiasi provisioning irreversibile.

## Prossimi incrementi

1. Congelare policy di ownership/auth e framing BLE, quindi parser con limiti e test sulle fixture JSON di tipo.
2. Aggiungere adapter di simulazione app–core con ACK di ricezione separato da commit; persistenza atomica e schema di migrazione.
3. Completare discovery, catalogo decoder, Flow Control e driver CAN/ISO-TP con TX subordinato a hardware/IGN qualificati, budget bus e test di simulatore.
4. Dimensionare immagine/asset/slot OTA prima di abilitare il buffer flash e recupero da interruzione.
5. Chiudere hardware e HAL power/display/ALS, poi prove di banco senza vettura.

Documentazione primaria consultata il 24/10/2026: [ESP Timer release 5.3](https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32s3/api-reference/system/esp_timer.html), [sorgente documentazione Espressif](https://github.com/espressif/esp-idf/blob/master/docs/en/api-reference/system/esp_timer.rst). Nessuna misura hardware o conformità dichiarata.
