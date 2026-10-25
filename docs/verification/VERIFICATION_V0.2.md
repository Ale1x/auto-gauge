# Verifica del secondo incremento parallelo

Nota successiva: la policy di ownership citata nello stato storico di questa verifica è stata superata da DEC-011. Sono ammessi più client in sequenza con una sola connessione attiva; il reset non serve per cambiare telefono. Riferimento corrente: `docs/protocols/CONNECTION_POLICY_V0.1.md`.

24 ottobre 2026. App, firmware e studio hardware svolti da tre subagenti con directory separate; coordinamento, riferimenti di protocollo e verifica integrata eseguiti nel repository comune. Nessuna pubblicazione o operazione sul gauge/vettura.

## Risultati riprodotti dal coordinatore

`python3 scripts/check_software.py` ha completato con successo:

| Controllo | Esito |
|---|---|
| Contratto configurazione | 22 casi comuni verificati da Python |
| Suite Python del repository | 25 test superati, inclusi interoperabilità C/Python e regressioni della review |
| Firmware host | Build Clang con ASan/UBSan; 2/2 eseguibili CTest superati |
| Swift package | 23 test superati, inclusi 22 casi config e15 vettori framing condivisi |
| App iOS | Build simulatore senza firma completata con Xcode27 |

Il subagente firmware ha inoltre verificato 13 vettori decoder, 10 gruppi diagnostici e10.000 ingressi casuali bounded. Sull'host: contesto RX320byte e scheduler1696byte; non sono misure ABI/memoria sul microcontrollore. Il codec C del framing è una libreria portabile separata in shared/c/, non ancora collegata al runtime ESP-IDF.

`python3 scripts/analyze_usb_power.py`, `python3 scripts/check_k0.py` e `git diff --check` hanno completato senza errori. I calcoli USB generano6casi OVP,36sensibilità fault,4inrush,3limiti corrente,2finestre preregolatore,240punti di potenza e4subtotal wake. I risultati mantengono esplicitamente HR-01/HR-02 non chiusi. Lo schema A0 non è stato modificato in questo incremento.

## Review e regressioni

Una review separata del riferimento di protocollo ha riprodotto tre problemi, tutti corretti e ricontrollati:

1. Due importatori potevano leggere un cursore fuori transazione e farlo arretrare. `BEGIN IMMEDIATE` ora precede la lettura; test con due connessioni e lock/retry.
2. Campi extra Unicode potevano essere applicati al simulatore e rendere la risposta successiva troppo grande. Ora il modello è normalizzato ai campi schema1, serializzato UTF-8 e verificato prima dell'apply; test apply→get.
3. Una pagina era serializzata prima di controllarne le dimensioni. Ora chiavi, cardinalità e lunghezze primitive sono verificate prima della serializzazione; test con campo da un milione di caratteri e controllo delle nuove allocazioni.

I18test di protocollo comprendono CRC/length/versioni, tutti gli split di un frame, massimo4096byte, timeout non prorogabile, errori di clock, config received/applied, resume dopo riapertura SQLite, duplicati, perdite dichiarate e rollback. Il revisore ha rieseguito questi test dopo le correzioni senza trovare problemi residui nei tre percorsi esaminati.

## Limiti effettivi

L'editor è stato compilato e testato tramite il suo core, senza nuova ispezione visiva del simulatore. I vincoli circolari restano warning di sviluppo. Il riferimento SQLite non è ancora integrato nella schermata storici e il simulatore config non è un device BLE. Non esiste un roundtrip fisico app–gauge.

ISO-TP implementa la ricezione Classic CAN/normal addressing su input sintetici, senza Flow Control TX, CAN FD, extended addressing, pacing STmin o tutte le condizioni normative. Lo scheduler restituisce un handoff simulato con guardia rivalutata, non invia frame sul bus. Solo sette PID hanno decoder; non è completato il requisito di tutti i PID Service01 standard. Nessuna compatibilità Polo dedotta.

ESP-IDF non compilato, nessun driver reale/OTA/flash log, nessuna eFuse o chiave. CRC e request ID non costituiscono autenticazione/antireplay. La policy ownership è ancora una proposta sottoposta al proprietario. Account/cloud, privacy e recupero reale non sono implementati.

Le due OVP USB dirette sono state respinte come soluzione completa. Preregolatore e alternative wake richiedono ancora dimensionamento/simulazione e prove. Nessuna corrente standby, protezione automotive, recovery alimentata USB o conformità è stata dimostrata dall'esito software.
