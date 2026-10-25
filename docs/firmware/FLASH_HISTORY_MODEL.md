# FW-08 — storico persistente su flash simulata

24 ottobre 2026. Modello C host di atomicità e recovery, compilabile ESP32-S3. **Non è il backend flash V1**: ogni append/begin-session ricopia il banco inattivo, con costo O(capacità) e metà spazio riservata alla copia. Nessuna scrittura hardware, partizione reale, radio, eFuse o chiamata da `app_main`. I 2MiB restano subordinati al sizing OTA, non congelato da questo formato.

## API e geometria di sviluppo

`ag_flash_history.h/flash_history.c` usano callback sincroni read/program/erase-sector con offset bounded. `ag_flash_history_sim.h/flash_history_sim.c` implementano soltanto RAM del chiamante, regola program1→0, erase4KiB e iniezione di guasto a una chiamata dopo un numero di byte. Il reset del simulatore conserva i byte e rimuove il guasto, modellando la successiva lettura dopo interruzione; non modifica flash reale.

Geometria H: numero pari di settori4KiB, da2 a512, totale8192…2097152byte. Due banchi uguali. Ogni banco ha header128byte, record128byte e marker finale4byte. Capacità configurabile1…floor((bank_bytes−132)/128):30record nella geometria minima,8190record al limite2MiB. Il formato è binario little-endian esplicito, mai struct/padding del compilatore. Header e record hanno CRC32/ISO-HDLC; CRC rileva corruzione accidentale e non autentica contenuti ostili.

`open` legge/valida entrambi i banchi, senza erase/default. `EMPTY` significa che tutta l’area è stata letta e trovata erased. Solo `bootstrap` esplicito su tale area può creare il primo snapshot con SID nonzero fidato e contatori iniziali. Un’area corrotta non viene confusa con storage vergine. I parametri first-sequence/first-serial sono metadati interni di recovery/test, non comandi accettabili dal telefono.

## Formato banco

| Byte header | Campo |
|---|---|
| 0–7 | `AGHS`, formato uint16=1, lunghezza header uint16=128 |
| 8–15 | Generazione snapshot uint64 positiva, mai wrap |
| 16–31 | SID corrente128bit |
| 32–39 | Capacità/count uint32 |
| 40–55 | Primo serial globale trattenuto / prossimo serial uint64 |
| 56–71 | Prossima sequenza della sessione corrente / ultimo tempo monotono uint64 |
| 72–79 | Totale record sovrascritti uint64 |
| 80–82 | Flag serial-exhausted, current-exhausted, current-has-records |
| 83–87 | Riservato zero |
| 88–103 | Serial globale iniziale / sequenza iniziale sessione corrente uint64 |
| 104–123 | Riservato zero |
| 124–127 | CRC di header0…123 |

Record in ordine cronologico globale dal byte128. Padding restante tutto0xff, escluso marker `HCM1` negli ultimi4byte del banco. I campi count/serial e ogni record vengono controllati prima di accettare lo snapshot; sequenze di record adiacenti dello stesso SID sono contigue e tempi monotoni. La sessione corrente non vuota è il run finale; contatori/ultimo tempo devono concordare. Generazione uguale con contenuto differente o metadati recenti che arretrano serial/perdite/sequence/time sono conflitto, senza selezione arbitraria.

| Byte record | Campo |
|---|---|
| 0–19 | `AGLR` + SID128bit |
| 20–43 | Sequenza per SID, tempo monotono e serial globale uint64 |
| 44–51 | ECU uint32, PID/status/raw-length uint8, riservato zero |
| 52–115 | Raw64byte, parte inutilizzata zero |
| 116–119 | CRC su record0…115 + record120…123 |
| 120–123 | Riservato zero |
| 124–127 | Marker `RL01` |

Raw fino64byte senza troncamenti. Una misura valida richiede raw non vuoto; uno status non valido richiede raw vuoto, come il modello `ag_history`. PID, ECU, status e lunghezze vengono validati. Serial/sequence/tempo sono interi uint64, non double/date assolute. Il valore UINT64_MAX può essere usato una volta; flag terminali impediscono wrap.

## Commit e power loss

1. Rileggere entrambi i banchi e verificare che lo snapshot attivo non sia cambiato.
2. Cancellare solo i settori del banco inattivo e verificare erased tutti i byte.
3. Scrivere header nuovo; copiare i record trattenuti dal banco attivo in streaming, saltando soltanto il più vecchio se la capacità è piena; aggiungere il record nuovo se presente. Ogni record è programmato come124byte più marker finale4byte.
4. Rileggere/validare header, tutti i record e padding mentre il marker banco è ancora erased.
5. Programmare `HCM1` per ultimo; rileggere/validare l’intero banco prima di dare successo.

Il banco attivo precedente non viene cancellato durante la transazione. Non esiste un metadata unico da considerare atomicamente valido dopo erase. Un’interruzione dopo il marker durevole ma prima dell’ACK può ripristinare lo snapshot nuovo: successo/errori del chiamante non sostituiscono il controllo di recovery. `open` sceglie la generazione integra più recente; una copia illeggibile o non erased ma invalida rende l’altra **READ_ONLY diagnostica**, perché la copia danneggiata potrebbe contenere dati più recenti. Entrambe invalide: nessun dato applicabile e nessun bootstrap automatico.

READ_ONLY non è un sync completo: la pagina può provenire dalla revisione precedente. Per un SID o cursore la cui retention non è dimostrabile restituisce READ_ONLY con output vuoto, invece di `NOT_RETAINED`/cursor-ahead falsamente definitivi. Errori I/O o corruzione durante query invalidano l’istanza e richiedono nuova apertura. Il modello non cancella automaticamente il banco incompleto per tornare scrivibile: riparazione esplicita sicura di media degradata è un incremento futuro. Questo limite può bloccare nuovo logging dopo un’interruzione a metà commit; non è dichiarata autonomia V1 qualificata dopo crank/power loss.

## Nuovi boot e sessioni precedenti

Una nuova istanza `open` non si riattacca al SID del boot precedente: append restituisce `NEW_SESSION_REQUIRED`. Il chiamante deve creare un **SID fresco fidato** per il boot e chiamare `begin_session`, che committa metadati nuovi preservando tutti i record trattenuti. Il tempo può ripartire solo nella nuova sessione; ogni SID ha la sua sequenza, mentre il serial globale ordina tutti i record. Nessun erase-all per cambiare boot, telefono o account.

SID corrente o ancora trattenuto viene rifiutato. Il core non mantiene un registro infinito degli ID totalmente evictati: il generatore fidato deve garantire freshness anche rispetto al passato non più trattenuto. Non usare reset del timer, MAC del telefono o contatore volatile ripartito da zero come fonte SID. L’entropia/identità boot reale resta HAL da qualificare; il modello non genera UUID finti come prova di unicità.

Query per SID scansiona record bounded e costruisce una finestra di massimo16record per il serializzatore `ag_history`, senza caricare il ring intero. Gap ritagliati al cursore derivano dalla prima sequenza ancora conservata di quella sessione; non viene applicato l’inviluppo globale delle perdite. SID interamente evictato, su media sana, restituisce `NOT_RETAINED`, **non una pagina vuota che sembri completa**. La nuova sessione corrente ancora vuota è distinta. La decimazione≤1Hz/PID del logger originario non viene duplicata da questo store: append accetta record simulati con tempi monotoni e il futuro adapter dovrà alimentarlo con la policy approvata. `overwritten` conta solo record precedentemente committati evictati dal ring, non campioni mai acquisiti/committati o overflow della coda RAM; i relativi errori e perdite restano segnali separati, senza inventare sequenze persistenti. ACK di A non elimina record e non conserva cursori globali di N telefoni; B può richiedere gli stessi dati ancora trattenuti.

## Costi, memoria e limiti

Al limite2MiB ci sono8190slot, indipendentemente dalla lunghezza raw effettiva. Con cinque PID ciascuno a1Hz sono al massimo1638secondi (27min18s) di record in questa ipotesi, non7ore; raw massimo occupa circa25% dello spazio totale, per formato fisso e duplicazione. Non è una stima della retention/endurance V1: ricopiare fino1MiB e cancellare un banco per ogni append è intenzionalmente inefficiente e non va collegato al dispositivo reale. Servono log strutturato/GC, wear budget e prove sul componente prima dell’implementazione finale.

Nessun heap e nessun array di milioni di record. Store host152byte; simulatore4680byte di controllo/trace, più storage RAM esterno fino2MiB. Scan/copy usano buffer128byte e pochi record/metadati locali. La query riusa `ag_history_snapshot_t` fixed-capacity (6200byte host) pur riempiendo al massimo16record, più staging4097byte del serializzatore e buffer del chiamante: il consumo stack annidato è significativo. Prima di runtime occorrono task/lock proprietario, stack misurato e dimensionato sul target, no callback rientranti e controllo sessione BLE prima dell’invio. La sonda GCC Xtensa della vera riga IDF `-Og` con `-fstack-usage` riporta frame statici6272byte per `ag_flash_history_make_page` e4752byte per `ag_history_make_page`, annidati, oltre a helper/libC/caller; query528byte, commit656byte, scan400byte e open512byte. Sono frame statici, non high-water/runtime o dimensione task sufficiente. Lo stack default di `app_main` non è qualificato. Nessun budget del controller flash o throughput5Hz è dimostrato.

`gauge_flash_history_tests` copre2267tagli I/O (ogni byte programmato inclusi header/marker, più confini read/erase), bootstrap parziale senza default, append/overwrite e begin-session, bitflip di ciascuno dei4096byte del banco recente, short-read, conflitti di generazione, exhaustion, raw64/errore65, nuovi boot e sessioni preservate/evictate, ACK di due client e output corto. Tutte le6fixture/12pagine condivise coincidono byte per byte con il produttore flash, inclusi precisioneoltre2^53 e serializzazione a budget4096. La suite host usa ASan/UBSan; ciò non dimostra atomicità di un componente flash reale.
