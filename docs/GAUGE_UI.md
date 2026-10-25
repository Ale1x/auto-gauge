# Controller gauge autonomo e scena tipizzata

`ag_gauge_ui.h` / `gauge_ui.c` implementano logica del gauge indipendente da telefono, renderer o periferiche. Una istanza serializzata per sessione conserva soltanto configurazione validata, dashboard selezionata e clock/debounce; nessun heap, flash, GPIO, pannello, font o framework grafico. Lo snapshot di ingresso deve restare immutabile per la chiamata e non sovrapporsi a controller/output.

## Configurazione e tap locale

Project valida l’intera configurazione tramite `ag_config_validate` contro la capability mask del chiamante. Adotta una copia canonica dei campi attivi; la scena contiene solo la dashboard selezionata e fino a5widget, con ID/PID e rettangolo normalizzato originale, senza cambiare la geometria dell’editor. Una revisione più recente preserva l’ID dashboard se presente, altrimenti seleziona la prima. Regressione di revisione o cambiamento semantico alla stessa revisione sono rifiutati. I campi inattivi/padding non definiscono uguaglianza della configurazione.

`ag_gauge_ui_tap` richiede un evento locale qualificato e la policy corrente; non è un opcode RPC e non deduce touch da un messaggio remoto. Ogni tap valido passa alla successiva dashboard con wrap1…3; una sola dashboard resta la stessa. Debounce in millisecondi è un parametro esplicito del chiamante (zero significa nessun debounce), non una nuova taratura prodotto. Il timer si aggiorna soltanto su tap accettati e non viene resettato per un apply configurazione. Tap non qualificati non mutano neppure il clock del controller.

## Origine, qualità e valori

Il proprietario fornisce fino a15binding univoci PID→ECU e fino a15misure. L’UI non sceglie implicitamente una sorgente fra ECU che espongono lo stesso PID. Nessun binding/lettura corrispondente produce un valore sintetico; duplicati ambigui e limiti superati sono input invalidi. PID non implementati restano UNSUPPORTED anche se un input capability scorretto li pubblicizza.

Un valore corrente richiede qualità VALID, `has_value`, raw bounded con lunghezza esatta, timestamp non futuro e età inferiore al parametro `maximum_sample_age_ms` positivo. Il raw viene nuovamente decodificato da `ag_obd_parse_measurement`: valore/unità cached del campione non sono autorità. Questo conserva le53formule razionali, gli zero validi e i valori frazionari/U32 senza clamp. La scena fornisce unità dal decoder e identità ECU numerica; non duplica o inventa53etichette localizzate o nomi ECU.

Gli stati UNSUPPORTED/STALE/TIMEOUT/ERROR/UNAVAILABLE restano distinti; una lettura valida al confine esatto dell’età massima diventa STALE. Timestamp futuro, forma raw errata o VALID senza valore producono ERROR. In tutti questi casi `has_value=false`, `value=NAN`, `unit=UNKNOWN`; il futuro renderer deve mostrare **`—` e indicatore di indisponibilità**, mai zero o valore precedente. Una qualità invalida non eredita neppure il numero cached dal campione. L’unità di una misura assente resta UNKNOWN perché l’API decoder attuale non espone metadata indipendenti dal raw; nessun getter artificiale viene sostituito con un campione zero inventato.

## Alimentazione e luminosità

La policy arriva dal proprietario hardware qualificato, mai da traffico CAN o tensione osservata inferiti dall’UI. IGN OFF, standby richiesto, alimentazione assente o USB-only richiedono OFF per il normale gauge. Power/IGN ignoti, rail/hardware non qualificati producono POWER_UNAVAILABLE; OBD/OBD+USB con rail/hardware qualificati e IGN ON producono ON. Questa API non introduce una modalità display manutenzioneUSB: va fornita separatamente dalla relativa policy locale se richiesta. Bus-off o sessione diagnostica inattiva non spengono il display ma impediscono di rappresentare letture cached come correnti.

Manuale usa esattamente la percentuale0…100 già validata nella configurazione: zero è un comando manuale valido. AUTO accetta soltanto `als_valid` con percentuale normalizzata0…100 fornita da un adapter fidato; non inventa curva lux, smoothing o fallback manuale. Se il dato manca/è fuori range, la scena dice BRIGHTNESS_UNAVAILABLE. Il campo percentuale zero in questo stato **non è un comando di luminosità**: un adapter non deve applicarlo o dichiarare un valore noto. Le decisioni su curva/limiti/priorità ALS restano esterne a questo controller.

## Errori e clock

Ogni project fallito pubblica una scena diagnostica vuota con power/brightness indisponibili, mai il precedente risultato. Regressione del clock monotono è terminale per l’istanza; una nuova sessione/boot/contesto veicolo richiede inizializzazione esplicita. I timestamp delle misure devono riferirsi alla stessa sessione del proprietario: `ag_measurement_t` non contiene un SID e questa UI non può autenticare campioni da boot precedenti.

## Verifica

`test_gauge_ui.c` passa6gruppi con ASan/UBSan e warning-as-error: zero autentico e ricodifica raw ignorando cache, tutte le qualità e stale/futuro, bindingECU/duplicati/bounds, tap/debounce/ciclo e revisioni con cambio/rimozione ID, alimentazione e ALS senza fallback, regressioni/UInt64 e passaggio attraverso tutti53decoder. Il sorgente cross-compila anche con il compilatore Xtensa fissato e warning-as-error. Nessuna prova pannello/touch/ALS fisico, consumi, rendering o leggibilità viene dedotta da questi test.

Integrazione CMake eseguita dal coordinatore:

```cmake
target_sources(gauge_core PRIVATE src/gauge_ui.c)
add_executable(gauge_ui_tests tests/test_gauge_ui.c)
target_link_libraries(gauge_ui_tests PRIVATE gauge_core)
target_compile_options(gauge_ui_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
add_test(NAME gauge_ui_tests COMMAND gauge_ui_tests)
```
