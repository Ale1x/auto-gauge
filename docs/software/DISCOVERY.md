# Discovery Service01 e proposta iniziale

`firmware/include/ag_discovery.h` / `firmware/src/discovery.c` forniscono un orchestratore a memoria fissa per un massimo di8coppie fisiche request/response già selezionate dalla piattaforma. Non rilevano automaticamente identità ECU, bitrate o formato. Sono supportate le quattro varianti CAN classico già ammesse dal core:11/29bit a250/500kbit/s. Una istanza usa un solo formato/bitrate e rifiuta indirizzi request/response duplicati e i noti ID request funzionali; nessuna mappa di indirizzi OEM viene dedotta dal payload.

## Sequenza e capacità

Init non trasmette o scrive flash. Prepare seleziona il prossimo lavoro; dispatch ricontrolla tutte le guardie CAN e costruisce esclusivamente Service01 con PID bitmap00/20/40/60/80/A0/C0/E0. Il risultato è lo stesso `ag_simulated_dispatch_t` del core, un handoff host: l’adapter deve ancora verificare la policy immediatamente prima del driver, notificare il completamento e usare `ag_discovery_tx_failed` se l’invio fallisce o è incerto. Non esiste API per servizi/byte CAN arbitrari.

Ogni ECU viene interrogata sequenzialmente a partire dalla pagina00. Le pagine successive vengono richieste soltanto se l’ultimo bit della risposta corrente le pubblicizza; E0 non fa wrap. Le bitmap si ricevono solo come ISO-TP Single Frame CAN classico, essendo la risposta positiva6byte, tramite il parser esistente. Nessun Flow Control o reassembly multiframe è necessario o autorizzato per questo percorso.

Una risposta di un’altra ECU/formato è ignorata senza prolungare il timeout. Una pagina positiva precedente/diversa da quella attesa è ignorata senza estendere la deadline assoluta. Negative Service01 (incluso response-pending), bitmap malformata o TX fallito chiudono quel candidato senza retry e passano al successivo. Il timeout parte dal dispatch e non si estende per traffico estraneo. Revoca di qualunque guardia o regressione temporale interrompono invece l’intera istanza terminalmente: riabilitare la policy non riesuma richieste preparate o attive.

I risultati conservano bitmap e `pages_seen` per ogni ECU, con esito COMPLETE/TIMEOUT/MALFORMED/NEGATIVE/TX_FAILED e NRC quando presente. Un errore su una pagina successiva non cancella pagine già validate: sono capacità **parziali realmente osservate**, non una discovery completa di ogni PID. I getter diventano disponibili soltanto quando tutti i candidati hanno terminato. L’unione utilizzabile interseca sempre le bitmap con l’allowlist effettiva dei53decoder scalari, senza promuovere bit di continuazione o PID non implementati.

## Default capability-aware

`ag_discovery_build_defaults` produce solo una proposta RAM, versione schema1/revisione1, dopo la discovery bounded. Il chiamante fornisce massimo dashboard1…3, widget per dashboard1…5 e percentuale manuale0…100. Queste opzioni sono un profilo esplicito di sviluppo e non congelano O-24 o la curva ALS.

La scelta ordina prima0C/0D/05/42/0F/10/11/04/0B/23/5C/46/2F/5E/A6 se realmente utilizzabili, poi i restanti PID implementati in ordine numerico senza duplicati. Se sono disponibili meno misure, si producono soltanto dashboard non vuote; se nessuna misura è disponibile, si restituisce EMPTY senza inventare dati o alterare l’output. Errori e NOT_READY lasciano l’output invariato.

Il layout di sviluppo usa righe intere nel quadrato normalizzato2000…8000, interno al cerchio; nessuna coordinata supera10000. La proposta è ricontrollata da `ag_config_validate` contro la stessa unione capability prima di essere pubblicata. Non apre il config store, non ripara uno store READ_ONLY e non effettua provisioning implicito: l’adozione iniziale persistente rimane un’operazione locale esplicita separata.

## Verifica

`firmware/tests/test_discovery.c` contiene6gruppi con ASan/UBSan e warning-as-error: tutte e4le varianti,2ECU×8pagine e allowlist53, continuazione/isolation/stale page, timeout esatto e UInt64 limite, negative/TX failure senza retry, input malformati, pagine parziali,18revoche pre/post handoff, regressione clock e default3×5/solo1PID/nessunPID con output transazionale. Nessuna trasmissione CAN fisica, compatibilità veicolo, bitrate auto-detection o copertura completa Service01 viene dichiarata.

Integrazione CMake eseguita dal coordinatore:

```cmake
target_sources(gauge_core PRIVATE src/discovery.c)
add_executable(gauge_discovery_tests tests/test_discovery.c)
target_link_libraries(gauge_discovery_tests PRIVATE gauge_core)
target_compile_options(gauge_discovery_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
add_test(NAME gauge_discovery_tests COMMAND gauge_discovery_tests)
```
