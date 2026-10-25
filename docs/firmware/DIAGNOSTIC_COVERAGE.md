# Copertura diagnostica scalare FW-09 — Service 01

24 ottobre 2026. **53 PID scalari implementati, non «tutti i PID standard».** Incremento da7a53 decoder, con raw associato e unità tipizzata. V1 segue DEC-003: sole letture live Service01, nessun OEM, servizio di scrittura/attuazione, Mode04, lettura DTC o VIN introdotti. Nessuna prova Polo, radio o trasmissione CAN fisica.

## Fonti primarie e metodo

Fonti OSS dei rispettivi autori, fissate ai commit letti:

- [python-OBD commands.py](https://github.com/brendan-w/python-OBD/blob/a378bdd81d58c67d08050e4244173a9a7dbda73d/obd/commands.py), [decoders.py](https://github.com/brendan-w/python-OBD/blob/a378bdd81d58c67d08050e4244173a9a7dbda73d/obd/decoders.py), [UnitsAndScaling.py](https://github.com/brendan-w/python-OBD/blob/a378bdd81d58c67d08050e4244173a9a7dbda73d/obd/UnitsAndScaling.py): associazioni PID, lunghezze e conversioni fino5F.
- [ELMduino.cpp](https://github.com/PowerBroker2/ELMduino/blob/e9d6b55fd63f4afd7789d0981861e9990cdfcd8e/src/ELMduino.cpp) e [header](https://github.com/PowerBroker2/ELMduino/blob/e9d6b55fd63f4afd7789d0981861e9990cdfcd8e/src/ELMduino.h): metodo processPID per i relativi segnali, rapporto44 e torque61–63. Considerati i parametri delle funzioni pubbliche, senza copiare helper con troncamenti interi.
- [Shiny.Obd OdometerCommand](https://github.com/shinyorg/obd/blob/4150556609970a1a67bc04d7f302a105abf7ce21/src/Shiny.Obd/Commands/OdometerCommand.cs): A6,4byte unsigned big-endian/10km; lo stesso autore richiede di verificarne il supporto e segnala revisioni successive dello standard.

Il testo normativo completo SAE/ISO non è stato acquisito: queste sono evidenze di implementazioni pubbliche, **non certificazione J1979/ISO né compatibilità dimostrata del veicolo**. Non è stato incorporato/vendorizzato codice esterno o aggiunta una dipendenza GPL/MIT; il decoder C e i test sono scritti qui, usando le formule numeriche come dati tecnici di riferimento.

Discrepanze risolte esplicitamente: python-OBD usa il fattore arrotondato0,0000305 per44; ELMduino esplicita2/65536. Qui si usa **1/32768**, con test32768→1. Per3C…3F e5D si preservano i decimali: processPID e python-OBD danno scale frazionarie, mentre alcuni helper ELMduino effettuano divisioni intere prima del cast. Non si importano quei troncamenti. I massimi riportati sotto sono calcolati dalle scale selezionate, non copiati da commenti approssimati.

## Matrice dei 53 decoder implementati

A è il primo byte dati; U16=256A+B; U32 è ABCD unsigned big-endian. Le lunghezze sotto sono **solo dati**, esclusi i due byte41/PID. Risposta positiva accettata solo se la lunghezza totale è esattamente dati+2: eventuale padding CAN deve essere già escluso dal livello ISO-TP.

| PID hex | Significato scalare | Byte | Conversione | Unità |
|---|---|---:|---|---|
|04|Carico calcolato|1|100A/255|%|
|05,0F,46,5C|Temperatura refrigerante, aspirazione, ambiente, olio rispettivamente|1|A−40|°C|
|06–09|Trim carburante corto/lungo, bank1/2|1|100A/128−100|%|
|0A|Pressione carburante|1|3A|kPa|
|0B,33|Pressione collettore, barometrica|1|A|kPa|
|0C|Regime motore|2|U16/4|rpm|
|0D|Velocità veicolo|1|A|km/h|
|0E|Anticipo accensione|1|A/2−64|gradi|
|10|Portata aria MAF|2|U16/100|g/s|
|11|Posizione farfalla|1|100A/255|%|
|1F|Tempo dall'avviamento|2|U16|s|
|21,31|Distanza con MIL, dalla cancellazione rispettivamente|2|U16|km|
|22|Pressione rail relativa al vuoto|2|79U16/1000|kPa|
|23|Pressione rail per iniezione diretta|2|10U16|kPa|
|2C|EGR comandato|1|100A/255|%|
|2D|Errore EGR|1|100A/128−100|%|
|2E,2F|Purge comandato, livello carburante|1|100A/255|%|
|30|Riscaldamenti dalla cancellazione|1|A|conteggio|
|3C–3F|Temperature catalizzatori B1S1/B2S1/B1S2/B2S2|2|U16/10−40|°C|
|42|Tensione centralina|2|U16/1000|V|
|43|Carico assoluto|2|100U16/255|%|
|44|Rapporto aria/carburante di equivalenza comandato|2|U16/32768|rapporto|
|45|Posizione relativa farfalla|1|100A/255|%|
|47,48|Posizione farfalla B/C|1|100A/255|%|
|49–4B|Posizione pedale D/E/F|1|100A/255|%|
|4C|Attuatore farfalla comandato|1|100A/255|%|
|4D,4E|Tempo con MIL, dalla cancellazione rispettivamente|2|U16|min|
|52|Percentuale etanolo|1|100A/255|%|
|59|Pressione rail assoluta|2|10U16|kPa|
|5A|Posizione relativa pedale|1|100A/255|%|
|5B|Vita residua pacco ibrido|1|100A/255|%|
|5D|Anticipo iniezione|2|U16/128−210|gradi|
|5E|Portata carburante|2|U16/20|L/h|
|61,62|Coppia richiesta/effettiva percentuale|1|A−125|%|
|63|Coppia di riferimento motore|2|U16|N·m|
|A6|Odometro riportato dalla ECU|4|U32/10|km|

Totale verificato dall'allowlist e dai test:53. Campo raw conservato per ogni misura valida:1/2/4byte, entroAG_RAW_CAPACITY8. Il decoder usa una tabella statica di coefficienti razionali, converte l'unsigned in double **prima** di scalare, quindi non moltiplica/tronca un intero32bit per coefficienti fisici.

Esempi di limiti numerici verificati: trim−100…99,21875%; carico assoluto0…25700% (non clampare a100);22fino5177,265kPa;23/59fino655350kPa;42fino65,535V;44fino1,999969482421875;3C…3Ffino6513,5°C;5Dfino301,9921875gradi;5Efino3276,75L/h;A6fino429496729,5km. Non sono limiti fisici di normale funzionamento né soglie allarme.

42 non è una misura garantita ai morsetti della batteria.44 è un valore **comandato**, non una misura lambda proveniente da un sensoreO2: non rinominarlo implicitamente come dato misurato.22/23/59 hanno significati di pressione diversi. A6 non viene sostituito con chilometri OEM o integrali di velocità se assente; non si promette che una Polo2011 lo esponga. Anche trim benzina, etanolo e pacco ibrido possono normalmente essere assenti sulla vettura target.

## Matrice dei confini non implementati

| Categoria | PID/servizi | Stato effettivo |
|---|---|---|
|Discovery, non misura scalare|00/20/40/60/80/A0/C0/E0|Bitmap gestite separatamente da capabilities; non abilitate come misure nel decoder/allowlist. Nessun wrap del bit finale versoPID0.|
|Stato/enum, non misura scalare semplice|01/02/03/12/13/1C/1D/1E/41/51/5F|Non decodificati come double; nessuna lettura DTC aggiunta perPID02. Una futura rappresentazione richiede semantica dedicata.|
|RAW/composto da modellare|14–1B,24–2B,34–3B,4F/50,55–58,64e altre strutture successive|Risposte multi-componente, status, limiti o selettori non proiettati arbitrariamente in un singolo valore. **Attualmente UNSUPPORTED, raw di output vuoto**, non un passthrough raw già disponibile.|
|Altri PID non nella tabella|Qualunque altro byte|Non implementato; la semplice presenza nella bitmap non lo rende usabile. Non si etichettano tutti questi numeri come OEM o nonstandard: alcuni sono standard ancora da coprire.|
|Fuori scope / nonstandard per questa V1|Servizi OEM22, Mode02/03/04/09, comandi raw/attuazioni|Nessun decoder/nuova API o richiesta introdotti. La risposta misura accetta solo41; il costruttore continua a generare soltanto richieste01.|

La categoria RAW indica **lavoro ancora necessario sul modello**, non una nuova capability per polling o UI. Mancano più componenti perPID, qualificatori, sentinel/availability specifici delle strutture più recenti e prova normativa completa. Non si dichiara «tutti i PID» né si pubblicizzano misure non implementate.

## Capability, guardie e unità

La nuova allowlist contiene esclusivamente i53PID della tabella, ma non costituisce supporto ECU. `ag_obd_usable_pids` resta intersezione fra bitmap dell'ECU e implementazione del decoder; il catalogo demo non è un'autorità. Ricevere/decodificare un payload non modifica la bitmap. Le richieste di misura continuano a richiedere bit di supporto e guardie CAN valide; USB-only e IGN ignoto restano bloccanti. `ag_can_tx_allowed` e il costruttore Service01 non sono modificati.

Unità aggiunte **in coda**, preservando i valori precedenti0…5: kPa, V, secondi, chilometri, rapporto, conteggio, gradi, minuti, L/h, N·m. Nessuna modifica alla forma wire della configurazione o degli storici. Catalogo, etichette e conversioni dell'app dovranno conoscere questi nuovi PID prima di proporli nell'editor; la presenza dell'enum C non li aggiunge automaticamente all'app.

Errori, lunghezze diverse, PID/servizio errati e negative response non restituiscono un valore vecchio: has_valuefalse, valueNaN, raw_length0/raw azzerato, stato esplicito. Metadati pid/ECU/timestamp del tentativo restano presenti; per7F/01 il NRC viene conservato come errore, senza promuoverlo a misura. Per dati corretti restano le formule pubbliche: non si inventa un valore sentinel per sostituire indisponibilità che non è stata documentata.

## Verifica eseguita

Build host CMake Debug temporanea `/tmp/auto-gauge-fw09.iO0QoB`, con ASan/UBSan del progetto; `gauge_diagnostics_tests` e `gauge_core_tests`: **2/2passati**, nessuna segnalazione.

La suite diagnostica comprende ora13gruppi:13vettori preesistenti;53PID con entrambi gli estremi numerici, unità/raw/metadati, ogni lunghezza errata fino11byte eSIZE_MAX, negative response e PID errato;26vettori intermedi indipendenti (incluse frazioni,32768→1 e odometro oltre24bit); confronto completo dei256PID fra decoder/allowlist; bitmap vuota che resta vuota dopo decode; discovery8pagine e intersezione finale53PID; richieste sempreService01, USB-only/IGN ignoto rifiutati per ogni nuovo PID. Conservati anche i10000frame malformati/random delle prove ISO-TP e scheduler già esistenti.

Non è una misura ECU/5Hz, una build target finale o una prova sulla Polo. Il coordinatore eseguirà build target e verifica integrata dopo il freeze degli altri filoni; runtime, radio e HAL restano separati.
