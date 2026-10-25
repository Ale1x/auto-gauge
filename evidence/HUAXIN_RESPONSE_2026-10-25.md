# Huaxin / Janie — riscontro del 25 ottobre 2026

## Provenienza e ambito

Handoff letto: baseline software c215a19, seguito da dbedf85 (handoff). I nove originali in `email-response/` sono stati forniti dall'utente come materiale ricevuto su WhatsApp. Data di acquisizione locale 25 ottobre 2026; il timestamp del messaggio WhatsApp non è visibile nelle immagini. Nessun nuovo messaggio, ordine, pagamento, esecuzione del programma fornitore o flash.

Gmail verificato tramite connettore:
- [Risposta Janie](https://mail.google.com/mail/u/?authuser=passarelli598%40gmail.com#all/1a16ea8142ede051), 24 ottobre 2026 ore 18:04:21 Europe/Rome: segnala email respinte e chiede WhatsApp/altro contatto per inviare i file tecnici. Nessun allegato in questo messaggio.
- Risposta dell'utente nello stesso thread, 18:06:09: fornisce i contatti alternativi. Non occorre reinviare la richiesta iniziale.
- Thread precedente `1a16d3a6a89c9b04`: riletti offerta E32 e nostro follow-up `1a16d72c214c2295`; quest'ultimo chiedeva precisamente quantità, codice, touch, trasporto e rettifiche tecniche.

## Conferme nello screenshot `origin (3)`

Dichiarazioni del fornitore, non misure o certificazioni:

| Voce | Risposta |
|---|---|
| Modulo | ESHX0175Y003AMT003, AMOLED circolare 1,75 pollici |
| Touch | On-cell, senza pannello touch esterno separato |
| Quantità e prezzo | Cinque campioni a USD10 ciascuno, totale prodotto USD50 |
| Trasporto | UPS USD50; il fornitore definisce la destinazione zona remota |
| Totale dichiarato | USD100 per cinque campioni e trasporto |
| Partenza | Entro 2–3 giorni dalla ricezione del pagamento; non tempo di consegna |
| Interfacce | MIPI e QSPI; inviate due sequenze di inizializzazione |
| FPC di conversione | Tre foto, accessori opzionali |

Il riferimento automatico al prodotto da 6,39 pollici è espressamente smentito. La risposta seleziona il codice ESHX; non dimostra una generica equivalenza di ogni variante HX. Revisione esatta dei cinque campioni e contenuto della fornitura degli FPC opzionali non sono esplicitati. IVA, dazi, sdoganamento, condizioni di resa e validità dell'offerta non sono separati: USD100 non è un costo finale garantito consegnato e sdoganato.

## Schema di collegamento V1

`ESHX0175Y003AMT003 V1 wiring diagram.pdf`: Poppler rileva due pagine, la prima contiene il circuito e la seconda risulta bianca alla renderizzazione. Entrambe ispezionate. Il semplice comando `file` indicava erroneamente 278 pagine. Cartiglio 2025/12/11; simboli H0175Y003AMT003 V1 e modello ESHX0175Y003AMT003. È uno schema di collegamento, non un datasheet completo corretto che sostituisce ogni tabella della V0.

| Pin | Segnale QSPI riportato |
|---|---|
| 1, 7, 12, 22–31 | GND (nel disegno QSPI anche le linee MIPI inutilizzate sono a massa) |
| 2 / 3 | TP-INT / TP-RST |
| 4 / 5 | TP-SDA / TP-SCL |
| 6 | TP-3.3V |
| 8 | IOVCC 1,65–3,3 V |
| 9 | VCI 2,7–3,6 V |
| 10 | VBAT 3,7–4,5 V |
| 11 | VCI-EN 2,7–3,6 V |
| 13 | MTP PWR, marcato non connesso |
| 14 | Reset, net scritto ERSRT |
| 15 | CS |
| 16 / 17 | QSPI-D0 / QSPI-D1 |
| 18 | SCL, clock QSPI |
| 19 / 20 | QSPI-D3 / QSPI-D2 (ordine da preservare) |
| 21 | TE |

La nota raccomanda IOVCC 1,8 V, VCI 3,3 V, VBAT almeno 3,7 V e VCI-EN 3,3 V; l'intervallo VBAT indicato sopra resta 3,7–4,5 V. Il pin 6 del touch separato è anch'esso annotato 3,3 V. Questi dati migliorano la tracciabilità ma non fissano il profilo scheda, le tolleranze reali, i livelli di ogni segnale o il sequencing.

## Inizializzazioni display

Letti entrambi i piccoli file di testo originali. Il file QSPI abilita `RC4 80` e imposta `R3A 55` (commento RGB565; 77 RGB888). Nel file MIPI queste due righe sono commentate. Entrambi riportano `RFE 00`, `R35 00`, `R53 20`, `R51 FF`, `R63 FF`, `R2A 00 06 01 D7`, `R2B 00 00 01 D1`, `R11`, `delay 60`, `R29`.

Interpretando gli estremi come inclusivi, le finestre sono X=6…471 e Y=0…465, entrambe di 466 pixel. Questa è una deduzione dalla sequenza, da verificare sul pannello. L'unità del delay non è esplicitata. Non sono forniti qui framing QSPI, frequenza massima, reset/power timing completo, gestione sleep/wake o specifica touch: i file non sono un driver ESP-IDF pronto.

## Archivio touch

Archivio 7z elencato con `tar`; letto soltanto il file INI mediante estrazione su stdout. Nessun eseguibile avviato e nessuna programmazione:
- `TouchICUtilityFactory.exe`, 9.614.336 byte: utility di fabbrica Windows.
- `AutoLoad/CPH_2407052_CST9217_DWS_HXC_H0175Y003AM_1.8V_5734_V1.0.1_693275EE.bin`, 32.640 byte: immagine firmware touch; contenuto binario non analizzato.
- INI omonimo, 13.560 byte: `ChipName=CST-9217`, `I2CAddr=0x5A`, `I2CCmdHead=0xD0`, `I2CSpeed=3` (commento: 400 kHz), `TransferProtocol=0` (I2C), `INTTrigMode=1` (fronte di discesa), `XReso=466`, `YReso=466`, `FwVersion=1.0.1`, `ProjectID=00005734`.

Sono parametri dell'utility di fabbrica, non una specifica verificata del protocollo applicativo. Non è chiarito se 0x5A rappresenti un indirizzo I2C a 7 bit o una convenzione dell'utility. `DownloadFirmware=1` conferma che avviare l'utility potrebbe programmare il touch.

Punto da chiarire: il nome firmware contiene 1.8V e l'INI imposta limiti di test alto di 1600–2000 mV per SCL/SDA/INT/RST, mentre lo schema annota TP-VCC=3,3 V. Alimentazione e livelli logici possono essere distinti: non è di per sé una contraddizione provata, ma non autorizza pull-up o GPIO diretti a 3,3 V. Serve conferma scritta del livello e della tolleranza per ciascun segnale e della versione preinstallata sui campioni.

## Disegni meccanici e limiti di lettura

Le tre immagini PNG senza estensione sono state ispezionate:
- `origin`: FPC3-H0143Y004MT003-1 V0, lunghezza indicata 60 mm, spessore 0,2±0,03 mm.
- `origin (1)`: FPC3-H0143Y002M V2, lunghezza indicata 26,65 mm; immagine ritagliata sul lato destro.
- `origin (2)`: FPC3-H0175Y003AMT006-1 V0, lunghezza indicata 38 mm, spessore 0,2±0,03 mm, contatti numerati 1/31.

La larghezza dei contatti è indicata circa 9 mm e quella del corpo circa 9,6 mm nelle immagini pertinenti. Sono FPC opzionali con codici differenti, non una distinta connettori o una conferma di intercambiabilità. Servono scelta dell'accessorio, orientamento dei contatti, pitch e accoppiamento esatti, disegni completi con tolleranze e inviluppo piegato.

Il DWG è riconosciuto come AutoCAD 2018/2019/2020 e conservato con hash; non è stato decodificato né confrontato con il PDF. Nessuna quota del pannello o footprint è stata ricavata dal DWG.

## Punti ancora aperti prima del freeze pannello

1. Revisione del campione e datasheet coerente: temperature V0 discordanti ancora non rettificate.
2. Sequenza e tempi di alimentazione/reset/spegnimento, correnti massime per ogni rail, disaccoppiamento e comportamento in standby.
3. Livelli logici QSPI e touch, pull-up e tolleranza a 3,3 V, condizioni di back-power quando un rail è spento.
4. Specifica del protocollo runtime CST9217 e conferma firmware già installato; il pacchetto factory non sostituisce il driver host.
5. Disegno completo del modulo, connettore mating identificato, FPC opzionale scelto e relativo costo/inclusione.
6. Condizioni commerciali residue: revisione/accessori, imposte e oneri, resa e durata dell'offerta.

Quantità/prezzo/trasporto ora sono dichiarati esplicitamente. Il gate hardware e O-16 restano aperti; nessuna modifica a schema, GPIO, driver o autorizzazione ai test. Gli originali restano untracked e non ignorati, come richiesto dall'handoff.

## SHA-256 degli originali

- `10820cdfbecddbaee395542217be4777adc2422f5181e9f8eca94cd19a531ff7` — `60107b0e72f03a9e7d47af9fdb237104_3741235234974223328_m.7z`
- `3c816c00b5d35ed8e0528faf1bcc6fccf98f0758c23d0c0aa7cf29dc027d95d4` — `ESHX0175Y003AMT003 V1 wiring diagram.pdf`
- `804883dc05a8b458d825551f6d25f6fba5ce1135c1bc4588a7900dd7aa63f83d` — `ESHX0175Y003AMT003 V1.dwg`
- `4071c51da20666a3560bf23a21f25461500d19997126f3c604d5c95ba645ccd3` — `after code for 1.75_QSPI.txt`
- `9747ade54bd271f012b476b8d7591c0b12468e5b8941f2b561bb4ea83a8cea3c` — `initial code 5300+1173（1.75_mipi）.txt`
- `9a391cbe308d68e2f67f0bc33a2128522ef45684a0c07c4bbffa531abbface19` — `origin`
- `b5c242d2619d0385e31a18971287071cc712573991c46f1a8e30c5836b4d862e` — `origin (1)`
- `0ecd7c7d8d746028e33b199177da16824fbe79035a57106d5d419d188482636c` — `origin (2)`
- `fbc83178e72ef5f32f02ebb2828a2988477ab9c552b37254a5e9312832c2fe9c` — `origin (3)`
