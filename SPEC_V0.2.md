# OBD-II Smart Gauge — Specifica di prodotto e hardware

**Versione:** 0.2 (24-10-2026)  
**Stato:** bozza tracciabile; **non autorizzata per rilascio PCB** finché non sono chiusi i gate indicati.  
**Principio di progetto:** gli agenti non inventano requisiti, valori limite, componenti, tempistiche o compatibilità. Ogni elemento marcato `[TBD]` è una decisione da prendere o un risultato da verificare.

## Cronologia modifiche

- **v0.2 — 24-10-2026 (decisione del proprietario):** il supporto a veicoli con impianto nominale a **24 V è escluso dalla V1**. Il prodotto è alimentato dalla presa OBD-II di **veicoli con impianto nominale a 12 V**. Aggiornati requisiti, connettore candidato, architettura di potenza, verifiche, test e task per Codex. **Resta obbligatorio proteggere l'elettronica dai transitori automotive di un impianto 12 V, che possono superare ampiamente i 12 V nominali.** Nessun altro requisito di prodotto è stato modificato.
- **v0.1 — 23-10-2026:** prima stesura.

## 0. Convenzioni

- **[F] Fissato**: requisito confermato dal proprietario del prodotto.
- **[D] Derivato**: necessità tecnica conseguente ai requisiti, da verificare in sede di progettazione.
- **[O] Aperto**: decisione necessaria non ancora specificata.
- **[H] Ipotesi**: candidata da analizzare; non costituisce decisione né autorizzazione.
- **[V] Verifica**: test, documento o preventivo ancora necessari.

## 1. Visione, ambito e non-obiettivi

### 1.1 Visione
Gauge rotondo AMOLED da 1,75 pollici per leggere e mostrare su un unico display fino a 5 misurazioni OBD-II standard per dashboard, fino a 3 dashboard, senza dipendere dalla presenza dell'iPhone. Interazione locale tramite tap; configurazione tramite app iOS nativa via BLE; account e sincronizzazione cloud per l'app.

### 1.2 Ambito V1 confermato

| ID | Stato | Requisito |
|---|---|---|
| PR-001 | F | Supportare **solo OBD-II su CAN, ISO 15765-4**; nessun protocollo legacy K-Line/J1850. |
| PR-002 | F | Elettronica alimentata esclusivamente da OBD-II, destinata **esclusivamente a veicoli con impianto elettrico nominale a 12 V**; il funzionamento continuo su impianti nominali a 24 V **non è supportato**. |
| PR-003 | F | Interrogare e rendere disponibili tutti i **PID OBD-II standard supportati dal veicolo**, non parametri proprietari. |
| PR-004 | F | Sola lettura diagnostica: sono ammesse richieste diagnostiche necessarie a ottenere letture; proibite cancellazioni, attuazioni e scritture/stati modificabili. |
| PR-005 | F | Veicolo del primo collaudo: **Volkswagen Polo V/6R, 2011, 1.2 TDI, 75 CV, 3 cilindri**. |
| PR-006 | F | Display acceso con quadro acceso; resta acceso se motore spento ma quadro acceso. |
| PR-007 | F | Gauge lasciabile collegato per settimane; riaccensione automatica senza intervento umano. |
| PR-008 | F | Porta **USB-C nascosta**, destinata solo a sviluppo/manutenzione e recupero firmware; nessuna alimentazione esterna per l'uso normale. |
| PR-009 | F | **Display circolare AMOLED 1,75 pollici**, touch, collegato tramite FPC a **un PCB completamente custom**. |
| PR-010 | F | Un tap in qualsiasi posizione sul display passa alla dashboard successiva. |
| PR-011 | F | Fino a **3 dashboard** e fino a **5 misurazioni per dashboard**. |
| PR-012 | F | **Cavo OBD-II di 2 m**, con connettore removibile **lato display**; non usare due schede funzionali separate. |
| PR-013 | F | Montaggio magnetico. Limite dimensionale richiesto: **max 1,75 pollici**, interpretazione fisica esatta da chiarire. |
| PR-014 | F | Luminosità regolabile da app sia manualmente sia automaticamente con **sensore di luce ambientale**. |
| PR-015 | F | Fornitore PCBA candidato: **JLCPCB**, ma non definitivo. |
| PR-016 | F | Prima tiratura: **5 unità**; obiettivo di spesa **40–70 EUR per unità assemblata**. |
| PR-017 | F | Output produttivo desiderato: **PCB assemblato con display e cavo OBD installati**. |
| PR-018 | F | Gauge pienamente operativo senza iPhone; iPhone solo per configurazione, sincronizzazione dei dati previsti e aggiornamento. |
| PR-019 | F | **Aggiornamento firmware OTA via BLE**. |
| PR-020 | F | Associazione **esclusiva a un iPhone per volta**. |
| PR-021 | F | App con **account e sincronizzazione cloud**, esclusivamente **iOS / Swift**, soltanto in italiano. |
| PR-022 | F | App non espone una schermata live: è un'app di configurazione; tuttavia registra sessioni/storici/log nell'app. Meccanismo di trasferimento da definire. |
| PR-023 | F | Dashboard con **layout completamente personalizzabile** (entro i limiti 3×5). |
| PR-024 | F | **Obiettivo di 5 Hz** per misurazioni rapide, compatibilmente con ECU, CAN, protocollo e numero di PID. |
| PR-025 | F | Dato non disponibile: mostrare `—` e indicatore di indisponibilità, non zero né valore precedente. |
| PR-026 | F | Dashboard predefinite al primo avvio con parametri standard **solo se supportati**. |

### 1.3 Esplicitamente fuori ambito
- Veicoli con impianto elettrico nominale a **24 V** (ad esempio molti camion e autobus); nessuna compatibilità operativa continua a 24 V richiesta.
- K-Line, ISO 9141, ISO 14230 e SAE J1850.
- PID specifici OEM, UDS proprietario, codifiche/attuazioni/reset ECU, cancellazione DTC.
- App Android e app macOS.
- Dipendenza dall'iPhone o dal cloud per visualizzazione live sul gauge.
- Alimentazione esterna permanente, power bank o batteria come requisito di funzionamento.
- Seconda centralina o scheda OBD separata posta all'estremità del cavo.
- Trasmissione cloud direttamente dal dispositivo (nessun Wi-Fi/cellulare richiesto); **eventuale uso radio Wi-Fi va escluso o autorizzato separatamente**.

### 1.4 Confini ancora da fissare
- «Tutti i PID standard» significa **PID live Service 01** o anche PID dei servizi 02/09 e lettura DTC? `[O-01]`.
- «Max 1,75 pollici» significa diametro del pannello, diametro esterno del case oppure diametro del PCB? `[O-02]`. Un display commerciale nominalmente 1,75 pollici potrebbe occupare quasi tutta la sezione disponibile.
- «Associazione esclusiva» è esclusività del pairing fisico BLE, proprietà cloud o entrambe? `[O-08]`.

## 2. Topologia fisica

```
Vehicle OBD-II port (J1962)
    | 2 m removable cable: protected power, ground, CAN_H, CAN_L
    v
Display-side detachable connector
    |
[ONE custom PCB: input protection -> automotive PSU -> MCU + external CAN transceiver]
    |                  |              |            |
  AMOLED FPC       touch I2C     ambient light  BLE antenna
    |
Round gauge enclosure + magnetic mount, USB-C service access
    ~ BLE ~
Swift/CoreBluetooth iPhone app -- HTTPS --> account/cloud services
```

PCB fisicamente unico. Il pannello AMOLED con FPC **non** è considerato una seconda scheda elaborativa. Scelta numero di layer, stackup, montaggio fronte/retro e posizionamento antenna solo dopo studio meccanico e RF.

### 2.1 Connettore veicolo
- Connessione diagnostica SAE J1962. Per ISO 15765-4 usare i **pin 6 CAN-H, 14 CAN-L, 16 alimentazione, 4 e/o 5 massa secondo la progettazione**.
- Non assumere che il pin 16 sia sotto chiave: è generalmente alimentazione permanente.
- Non usare i pin discrezionali OEM per simulare un ingresso IGN.
- **Valutare connettore diagnostico SAE J1962 Type A**, destinato a prese OBD-II dei veicoli a 12 V; richiedere datasheet, dimensioni, pinout e verifica di compatibilità fisica con le prese dei veicoli target. Non introdurre Type B come requisito.
- Non aggiungere una terza terminazione CAN da 120 ohm in parallelo alla rete veicolo; verificare la topologia e il comportamento del ramo di 2 metri con test EMC/integrità del segnale.
- Cavo: coppia intrecciata per CAN_H/CAN_L, conduttori di sezione adatta alla corrente massima, protezione meccanica e strain relief; materiale, schermatura, pinout e connettore display-side **[TBD]**.
- Nessun supporto richiesto a prese diagnostiche o reti tipiche dei veicoli pesanti a 24 V (es. J1939/9-pin); il requisito diagnostico rimane soltanto ISO 15765-4 sui veicoli a 12 V.

## 3. Alimentazione e gestione dello stato

### 3.1 Requisiti
- [F] Funzionamento su impianti nominali a **12 V** dal pin 16 OBD-II; **non** richiedere funzionamento continuo con alimentazione nominale a 24 V.
- [F] Display acceso con quadro acceso, anche a motore fermo.
- [F] Standby sicuro per connessione continuativa di settimane.
- [D] Protezioni per sovratensioni/transitori automotive **dei sistemi nominali a 12 V**, ESD, corto circuito, polarità inversa, hot plug, sottotensione e avviamento. Il dimensionamento deve comunque considerare **picchi transitori superiori a 24 V**, se previsti dai profili di prova selezionati: questo **non** implica supporto al funzionamento continuo su veicoli a 24 V. Riferimenti da valutare: ISO 16750-2 e ISO 7637-2 (specificare esattamente severità e procedure prima di dichiarare conformità).
- [D] Assenza di backfeed dal VBUS USB-C verso la rete veicolo e viceversa; i casi d'uso USB con e senza OBD collegato sono `[O-03]`.
- [D] Spegnimento o isolamento efficace di display, periferiche e transceiver quando in standby; mantenere una sorgente di wake appropriata solo se giustificata.
- [D] Non mantenere la rete CAN del veicolo sveglia durante lo standby; evitare richieste OBD periodiche a quadro spento.

### 3.2 Rilevamento quadro: blocco prioritario
Non esiste sui pin OBD-II standard un segnale IGN dedicato e universale. Una misura di tensione batteria **non** prova che il quadro sia acceso, in particolare con motore spento, alternatore fermo o batteria sotto carica. Anche la presenza di traffico CAN non implica sempre accensione del quadro; un gateway può filtrarlo o continuare a trasmettere a quadro spento.

**Gate HW-IGN:** valutare mediante bench test sulla Polo almeno una strategia non proprietaria (ascolto CAN, richiesta diagnostica solo in uno stato attivo controllato, monitoraggio alimentazione, transceiver wake, timeout). Documentare condizioni di successo/fallimento e corrente in standby. Se il requisito «quadro acceso» non è realizzabile universalmente senza compromessi, richiedere decisione esplicita sul comportamento compatibile anziché inventare una regola.

### 3.3 Target quantitativi ancora aperti
`[O-04]` Assorbimento massimo standby e relativo metodo di misura.  
`[O-05]` Consumo in funzionamento e potenza termica.  
`[O-06]` Tempo massimo wake -> dashboard visibile, e timeout per standby.  
`[O-07]` Severità automotive e temperatura operativa/di stoccaggio.

## 4. Elettronica candidata, non congelata

| Blocco | Requisito | Candidato / verifica |
|---|---|---|
| MCU + BLE | Elaborazione UI, OBD, GATT e OTA | [H] ESP32-S3, per TWAI e BLE; modulo o SoC e PSRAM/flash `[TBD]`. TWAI richiede transceiver esterno. |
| Physical CAN | ISO 11898-2 per veicoli a 12 V, protezione automotive sui segnali CAN | [D] Transceiver adatto al bus, con standby/wake se necessario; componenti e caratteristiche `[TBD]`. |
| Connettore | Automotive, 2 m, removibile lato gauge | [D] Presa compatta con blocco meccanico, pinout, correnti e ingombro verificati. |
| Regolazione | Alimentazione nominale 12 V e relativi transitori automotive | [D] Stadio TVS/protezioni + alimentazione switching automotive. Topologia/limiti `[TBD]`. |
| AMOLED | Circolare 1,75", touch capacitivo | [H] Display della classe 466×466, CO5300 QSPI + CST9217 I2C, da confermare su **pannello nudo acquistabile**, non solo su devboard. |
| Ambient light sensor | Brightness automatica | [D] Sensore ottico con apertura sul case e posizione senza ostruzioni. IC `[TBD]`. |
| USB-C | Solo service/debug/recovery | [D] Protezioni dati/ESD, sequenza boot e path VBUS; nessun impiego come alimentazione normale. |
| RF | BLE affidabile entro il case | [D] Antenna, keep-out, match, RF testing; magneti/metallo non devono compromettere la radio. |
| Storage | Dashboard/config; due partizioni OTA, sicurezza; esigenze log da definire | [D] Flash sufficiente al firmware e all'aggiornamento di recupero. Formato e quantità `[TBD]`. |

### 4.1 Studio dimensionale obbligatorio
- [F] Case con montaggio magnetico e «max 1,75 pollici».
- [V] Ottenere disegni del pannello, cover lens, FPC, area visibile e area meccanica, posizione connettore USB e presa del cavo, RF keep-out, spessore PCB, altezza componenti, sensore luce, magneti, strain relief.
- [V] Produrre una sezione quotata dell'assieme e un disegno PCB quotato prima dello schematico definitivo.
- [O] Se il pannello nominale 1,75" non consente di rientrare in **diametro esterno** 44,45 mm, ottenere decisione esplicita sulla tolleranza ammessa. Non ridimensionare autonomamente il requisito.

## 5. Stack diagnostico

### 5.1 Fisico e rete
- CAN OBD ISO 15765-4 con rilevamento/negoziazione delle varianti previste dallo standard: **11/29-bit ID, 250/500 kbit/s** dove applicabile.
- ISO-TP per messaggi multiframe dove necessari.
- Supporto di risposte multiple da ECU differenti, evitando duplicati e associare ai dati un'origine ove necessario.
- Discovery delle capability via richieste support PID (e.g. 01 00, 01 20 e pagine successive) in base al protocollo e servizi definiti nel perimetro `[O-01]`.
- Gestione timeout, Negative Response/nessuna risposta, bus-off, perdita comunicazione, ritardi, sospensione e riattivazione.
- Non eseguire richieste diagnostiche a quadro spento durante standby che possano risvegliare ECU o gateway.
- Ripartizione della bandwidth: non assumere 5 Hz per ciascuno di 15 PID. Rate differenziati e budget delle interrogazioni obbligatori.

### 5.2 Divieti diagnostici
- Consentire SOLO messaggi di interrogazione necessari alla lettura standard richiesta.
- Vietare esplicitamente richieste che cancellano DTC (Mode 04), modificano codifiche, comandano attuatori o scrivono configurazioni nelle ECU.
- Nel firmware implementare allowlist dei servizi diagnostici, non limitarsi a disabilitare controlli UI.

### 5.3 Catalogo misure e stato
Per ogni PID standard supportato: `service`, `pid`, `ecu_source` se rilevante, `name_it`, `unit`, `raw_bytes`, `decoder`, `min/max` se da specifica, `last_updated_monotonic`, `status` (SUPPORTED/UNSUPPORTED/STALE/TIMEOUT/ERROR), `sampling_target_hz`. Evitare di usare valori numerici fittizi per dati non disponibili.

**Stato UI:** `—` con indicatore visuale per qualunque misura temporaneamente indisponibile, mai ultimo dato presentato come live.

### 5.4 Scheduler (5 Hz)
- [F] Obiettivo di 5 Hz per *misure rapide*.
- [D] Frequenza per PID configurabile dal firmware secondo ECU e impatto bus; priorità per misure visibili; report della frequenza **effettiva**, non solo quella richiesta.
- [V] Test con 1, 5 e 15 PID attivi e log timestamps ECU; criterio di accettazione quantitativo in `[O-09]`.

## 6. UI embedded e configurazioni

### 6.1 Navigazione
- 1–3 dashboard attive.
- 1–5 misure per dashboard.
- Tap ovunque cambia dashboard; dall'ultima torna alla prima.
- Persistenza dashboard e luminosità dopo power cycle.
- Avvio con dashboard predefinite composte solo da PID disponibili; mancanza dati segnalata senza crash.
- Limiti di validazione **sia nell'app sia nel firmware**.

### 6.2 Editor layout
- [F] Layout completamente personalizzabile da iPhone, entro 3×5.
- [O-10] Modello editor: coordinate libere, griglia, widget predefiniti, dimensioni consentite, sovrapposizioni, soglie/colori/stili e font.
- [D] Schema versionato con dimensioni normalizzate indipendenti dai pixel, schema migration e checksum/versioning; applicare config solo se valida; rollback alla config precedente se upload fallisce.
- [D] Anteprima editor con resa coerente al display effettivo.
- [O-11] Effetto di un PID che era supportato ma diventa indisponibile su un'altra auto (nascondere, mantenere con `—`, riempire automaticamente?).

### 6.3 Luminosità
- [F] Manuale e automatica con sensore ambiente.
- [O-12] Priorità modalità manuale/auto, range/curva, min/max e limiti antiriflesso, persistente tra boot; comportamento durante l'OTA.
- [D] Filtraggio per evitare oscillazioni con variazioni rapide della luce.

## 7. BLE, app Swift e cloud

### 7.1 BLE
- [F] Configurazione, sincronizzazione necessaria e OTA via BLE; nessuna dipendenza da smartphone per funzionare in auto.
- [D] Protocollo GATT **versionato**, con device identity, sessione autenticata, info firmware/hardware, capabilities, update config, conferma di applicazione, battery/voltage status solo se definito, download log e DFU/OTA.
- [F] Un solo iPhone associato alla volta.
- [O-08] Pairing esclusivo, reset ownership/factory reset, dispositivo usato e telefono perso.
- [D] Cifratura autenticazione e antireplay per configurazione e aggiornamenti; secure boot/firmware firmato o meccanismi equivalenti validati.

### 7.2 App iOS
- [F] Swift, iPhone, lingua italiana.
- [F] Login/account, gestione del dispositivo, discovery BLE, pairing, catalogo PID supportati, editor delle 3 dashboard, brightness manuale/auto, stato firmware, aggiornamento OTA, storici/log/sessioni e sync cloud.
- [F] **Nessuna dashboard live** sull'iPhone; può ricevere telemetria per registrare sessioni soltanto secondo sincronizzazione approvata.
- [O-13] Requisiti iOS minimi, provider account, backend/cloud, cancellazione account, policy offline, protezione dati/VIN e conservazione sessioni.

### 7.3 Contraddizione operativa da risolvere
L'app deve salvare storici/log, ma il gauge deve funzionare senza iPhone e non è previsto upload autonomo. Occorre scegliere fra:
- buffer delle sessioni sul gauge, poi sync BLE con iPhone;
- registrazione sul solo iPhone **quando l'app è realmente connessa**;
- nessuna garanzia di storico continuo quando l'iPhone è assente.
La scelta determina memoria, schema dati, consumo, clock, OTA e BOM. **[O-14]**.

### 7.4 OTA
- [F] OTA via BLE.
- [D] Immagine firmata/verificata, compatibilità hardware, processo interrompibile, watchdog, fallback a immagine funzionante, progress reporting all'app; partizioni sufficienti e test interruzione alimentazione.
- [O-15] Download app/cloud e policy aggiornamenti; versione bootloader e recovery via USB.

## 8. Obiettivi non funzionali

| Categoria | Requisito | Stato |
|---|---|---|
| Affidabilità | Boot e riconnessione autonomi, watchdog, fault recovery | D; timeout numerici O |
| Durabilità batteria | Standby per settimane senza scarica rilevante | F qualitativo; corrente quantitativa O |
| Aggiornamento | BLE OTA con recovery verificato | F + D |
| Privacy | Account e cloud con gestione dei dati personali e del device | D; policy O |
| Prestazioni | 5 Hz per PID rapidi ove supportato | F come obiettivo; criterio numerico O |
| Ambientale | Range temperatura, condensa, vibrazione, ingress protection | O |
| Sicurezza elettrica | Protezione da eventi transitori sui sistemi nominali a 12 V | D; profili di test O |
| EMC/RF | Antenna BLE affidabile e nessuna interferenza veicolo | D; standard/limiti O |
| Meccanica | Montaggio magnetico sicuro, niente interferenza con comandi e airbag | D; metodo O |
| Produzione | 5 pezzi assemblati, obiettivo 40–70 €/unità | F; validità economica V |

## 9. Test e criteri di accettazione proposti per la revisione

Questi sono **test da eseguire**, non dichiarazioni che il dispositivo li supera. I limiti mancanti restano aperti.

| Test ID | Setup | Criterio |
|---|---|---|
| T-001 | Alimentatore automotive a 12 V nominali | Avvio e telemetria su simulatore senza anomalie |
| T-002 | Simulazione avviamento e sottotensione su impianto nominale a 12 V; livelli `[O-07]` | Comportamento conforme ai limiti approvati, senza danni né alimentazione anomala |
| T-003 | Test supply transients scelti ISO 16750-2 / 7637-2 | Nessun danno/comportamento vietato entro livello approvato |
| T-004 | Collegamento 2 m a veicolo/simulatore | CAN comunicante; nessun errore/degrado significativo sulla rete |
| T-005 | Polo 6R, quadro ON/motore OFF | Display acceso e lettura dei PID disponibili |
| T-006 | Polo 6R, quadro OFF, per varie ore | Nessun polling risvegliante; assorbimento sotto limite `[O-04]` |
| T-007 | Veicolo spento e successivamente quadro ON | Wake automatico entro limite `[O-06]` |
| T-008 | Discovery PID CAN 11/29bit 250/500k su simulatori | Lista accurata, decoders corretti, timeout gestiti |
| T-009 | 1,5,15 misure richieste | Raggiungimento obiettivo realistico 5 Hz sui PID rapidi entro `[O-09]` |
| T-010 | Tocchi sul display | Sequenza dashboard 1→2→3→1, rispettando il numero configurato |
| T-011 | App editor BLE | Config valida persiste; config non valida è rifiutata |
| T-012 | Sensore luce/controllo app | Auto e manuale funzionano secondo curva `[O-12]` |
| T-013 | Telefono non presente | Dashboard funzionante senza connessione BLE/cloud |
| T-014 | App iPhone autorizzata e non autorizzata | Solo owner può configurare; recupero verificato |
| T-015 | OTA interrotta su diverse percentuali | Rollback/recovery, nessun brick |
| T-016 | Sessioni/log | Sync e perdita dati valutati secondo `[O-14]` |
| T-017 | Connettore cavo removibile, vibrazione e tiro | Affidabilità conforme ai requisiti meccanici definiti |
| T-018 | Verifica BOM, PCB, pannello e accessori | Preventivo scritto per 5 dispositivi completi, target unitario valutato |

## 10. Progetto KiCad: processo e gate

### Fase K0 — Fattibilità obbligatoria PRIMA dello schematico
1. Confermare datasheet e approvvigionamento pannello AMOLED **nudo** + touch + FPC; non confondere devboard e pannello.
2. Congelare definizione geometrica di «max 1,75 pollici» e disegno ingombri preliminare.
3. Verificare JLCPCB/altro fornitore per PCB, FPC insertion, cavo 2 m, connettore, flashing, test; acquisire preventivo preliminare.
4. Verificare connettore veicolo 12 V (Type A candidato) e connettore removibile lato display con 2 m di cavo.
5. Proporre architettura affidabile per wake/standby non proprietaria, con test di fattibilità su auto/simulatore.
6. Definire corrente standby e grado di protezione da transitori; quantificare BOM e target costo.
7. Definire come funzioneranno i log in assenza dell'iPhone per determinare storage/RTC.
**Exit gate:** report fattibilità, BOM provvisoria verificata, disegno quotato e lista decisioni bloccanti risolte. Nessuna fabbricazione prima dell'exit gate.

### Fase K1 — Scelta componenti e architettura elettrica
- Diagramma a blocchi, interfacce, segnali, rail e power budget con margine.
- Componenti con **MPN** produttore, datasheet originale, codice magazzino assemblatore se presente, disponibilità MOQ e componenti alternativi compatibili.
- Compatibilità logica GPIO/voltaggi, correnti, temperature, boot strapping, memorie OTA e pin liberi.
- Schema CAN transceiver e protezioni, no 120 Ω di terminazione fissa; stub 2 m validato.
- Analisi sicurezza corrente dalla porta OBD, fusibile/protezioni e transitori/load dump relativi a sistemi nominali a 12 V; non progettare la compatibilità operativa nominale 24 V.
- Scheda capacità RF/magneti/case, ingressi touch/sensore luce.
**Exit gate:** revisione ingegneristica componenti + power/risk calculations.

### Fase K2 — KiCad Schematic
Sheet consigliati, soggetti a verifica:
- `01_OBD_INPUT` (pinout connettore/cavo, ESD, ground strategy, ingress protection).
- `02_AUTOMOTIVE_POWER` (protections, buck/rails, sequencing, low-power).
- `03_MCU_MEMORY_BLE` (modulo/SoC, antenna, clock, boot, test/USB).
- `04_CAN_PHY` (TWAI external transceiver, standby/wake, protection).
- `05_AMOLED_TOUCH` (pinout FPC da datasheet verificato, rails, QSPI/I2C).
- `06_LIGHT_SENSOR` (ottica, I2C/IRQ se previsto).
- `07_TEST_SERVICE` (USB-C service, buttons/test pads, recovery).

Deliverable: `.kicad_pro`, `.kicad_sch`, simboli custom **con datasheet**, netlist/export, pin assignment matrix, ERC pulito (o eccezioni motivate) e check di peer-review.

### Fase K3 — PCB layout
- Partenza da meccanica e stackup fabbricabile, forma reale dell'assieme.
- Considerare **4 layer solo come candidato**, non come decisione: verificare costi, ritorni di massa, RF e integrità QSPI/CAN.
- Placement: modulo RF/antenna e keep-out magnetico/metallico; supply switching lontano da RF, touch e ambient light sensor.
- Instradamento CAN e connessione 2 m: no stubs non verificati, EMI, ESD close to connector, ritorni.
- Routing QSPI al display e I2C touch/sensore luce secondo datasheet, FPC bend radius e assembly.
- USB-C posizionata per accesso manutenzione senza violare dimensioni o antenna; anti-backfeed.
- Test point accessibili, orientamento polarità, fiducial, clearances, panelization.
- DRC/board review con render 3D e sezioni quotate.
**Exit gate:** mechanical fit approvato, DRC pulito (o eccezioni motivate), revisione PCB indipendente.

### Fase K4 — Manufacturing package
- Gerber RS-274X/ X2 come richiesto, drill, fab drawing, stackup e board outline.
- BOM con MPN + codice JLCPCB, passivi e componenti DNP distinti.
- CPL/Pick-and-place, assembly drawing fronte/retro, orientamenti verificati, 3D STEP.
- Pinout cavo, plug J1962 Type A candidato per 12 V, scelta gauge connector, istruzioni FPC e strain relief.
- Istruzioni flashing/calibrazione e procedure di test post-assemblaggio, se confermate dal produttore.
- Preventivo scritto completo per 5 pezzi (distinguere PCB, PCBA, display, cavo, connettori, lavoro manuale, shipping, tasse e setup).
- Documentare componenti non montati dal produttore e costo/fase necessaria per la loro installazione.
**Exit gate:** revisione DFM/DFT e ordine solo previa approvazione.

### Fase K5 — Bring-up
1. Visivo e prova corti/continuità con nessun veicolo collegato.
2. Alimentazione limitata in corrente su banco e check rail; prove su sistema nominale a 12 V, sottotensione/avviamento e transitori nei livelli approvati, senza test di funzionamento nominale a 24 V.
3. USB-C programming, boot e log; verifica display/touch/ambient light sensor.
4. BLE, sicurezza, bonding, OTA e recovery.
5. CAN transceiver/OBD su simulatore protocollo, senza veicolo.
6. Verifica assorbimento standby e wake; prova non risveglio rete.
7. Solo dopo: prova sulla Polo 6R, acquisizione PID supportati e confronto con scan tool affidabile.
8. Registrare misure, anomalie, foto e revisioni KiCad prima di una seconda tiratura.

## 11. Questioni aperte prioritarie (non risolvibili dagli agenti senza consenso)

| ID | Priorità | Domanda / decisione |
|---|---|---|
| O-01 | P0 | Scope preciso di «tutti i PID standard»: solo Service 01 oppure anche Service 02/09, DTC lettura? |
| O-02 | P0 | Max 1,75": diametro esterno finito, diametro PCB o area schermo? Tolleranza in millimetri? |
| O-03 | P1 | USB-C alimenta temporaneamente il PCB per recovery senza OBD o richiede OBD collegato? Come prevenire backfeed? |
| O-04 | P0 | Assorbimento massimo consentito in standby misurato su impianto nominale a 12 V, e dopo quanti minuti dal quadro OFF? |
| O-05 | P1 | Budget di potenza/temperatura accettabile in funzionamento. |
| O-06 | P0 | Tempo di wake e timeout standby; comportamento in caso di CAN attivo con quadro OFF. |
| O-07 | P1 | Temperature e profili automotive; vibrazione, umidità, esposizione solare, grado IP. |
| O-08 | P1 | Esclusività pairing, factory reset, riassegnazione owner e iPhone perso. |
| O-09 | P1 | 5 Hz vale per quanti PID contemporanei e quale tolleranza quando ECU lenta? |
| O-10 | P1 | Specifica tecnica «layout completamente personalizzabile». |
| O-11 | P2 | Dashboard configurata per veicolo A usata su veicolo B con PID non disponibile. |
| O-12 | P2 | Brightness auto/manuale: logica precedenze, curva, limiti e override. |
| O-13 | P1 | Provider cloud, account, autenticazione, policy privacy e offline, retention storico. |
| O-14 | P0 | Origine e permanenza dei log se iPhone non è connesso: storage locale sul gauge oppure storico solo in app quando connessa. |
| O-15 | P2 | Distribuzione firmware e recovery/security policy. |
| O-16 | P0 | Pannello AMOLED nudo realmente approvvigionabile con FPC/pinout e vincoli dimensionali. |
| O-17 | P0 | Chi monta connettore lato gauge, il cavo e il display e a che prezzo? |
| O-18 | P0 | Meccanismo verificabile per «quadro acceso», senza dipendenze OEM. Quale compromesso è ammissibile? |
| O-19 | P1 | Budget 40–70 EUR include IVA, trasporto, attrezzaggio, cavo/display e montaggio o solo BOM+PCB? |
| O-20 | P1 | Il prodotto è prototipo privato o sarà venduto? Impatta prove EMC, RED/CE e conformità automotive. |
| O-21 | P1 | Connettore removibile lato gauge: orientamento, sporgenza e trattenuta meccanica. |
| O-22 | P1 | Tipo magnete/supporto, geometria e distanza antenna BLE. |
| O-23 | P2 | Target versione minima iOS, provider login, gestione account. |
| O-24 | P2 | Predefinite: 1 o 3 dashboard, metriche e layout iniziali quando supportati. |
**O-25 chiusa (v0.2):** il funzionamento nominale su veicoli a 24 V è fuori ambito. Nessuna ulteriore decisione relativa al supporto 24 V è richiesta per la V1.

## 12. Matrice rischi e dipendenze

| Rischio | Impatto | Azione richiesta |
|---|---|---|
| Wake del quadro non universalmente osservabile | Molto alto | Studio e test prima del PCB |
| Pannello 1,75" + case 1,75" + connettori + magnete | Molto alto | CAD ingombri e sourcing pannello |
| Transitori, load dump e avviamento su impianto nominale 12 V | Molto alto | Definire profili e protezioni automotive prima dello schematico |
| Cavo CAN 2 m e connettore lato display | Alto | SI/EMC e test, distinta cavo |
| Consumo continuo dalla batteria dell'auto | Molto alto | Budget standby misurato, stop polling a motore/quadro spenti |
| Costo 40–70 €/pc completamente assemblato | Alto | Quotazione PCBA + display + cavo + manodopera completa |
| Log su iPhone senza presenza iPhone | Alto | Definire storage e sync prima della flash BOM |
| BLE compromesso da magnete/metallo | Alto | Layout RF con RF keep-out e test |
| 15 PID e target 5 Hz | Medio-alto | Scheduler e misure performance reali |
| OTA senza recovery e autenticazione | Alto | Secure update/rollback con partizioni adeguate |
| Uso commerciale senza certificazioni previste | Alto | Scoping regolatorio con esperto |

## 13. Non-go decisionali per Codex

Codex **non deve**:
1. Dichiarare la Polo compatibile finché non è testata la vettura effettiva.
2. Selezionare a caso un connettore FPC/pannello sulla base di foto marketing.
3. Inserire una resistenza da 120 Ω sulla presa OBD come se fosse un'estremità CAN da terminare.
4. Aggiungere K-Line/J1850, un gateway wireless, una batteria o una seconda scheda senza requisito.
5. Interpretare `read-only` come `listen-only`: richieste diagnostiche autorizzate sono necessarie.
6. Assumere che l'attività CAN o un livello di tensione equivalgano sempre al quadro acceso.
7. Garantire 5 Hz per ogni PID simultaneo o stimare dati in assenza di risposta.
8. Dichiarare «assembla JLCPCB» senza verifica written quote per display/cavo.
9. Dichiarare il PCB pronto in base ai soli ERC/DRC.
10. Ordinare il PCB se esistono questioni P0 aperte, a meno di deroga esplicita documentata dal proprietario.

## 14. Riferimenti tecnici (raccolti per la v0.1; verifiche specifiche dei componenti ancora necessarie)

- Espressif TWAI external transceiver: https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32s3/api-reference/peripherals/twai.html
- Waveshare round 1.75 reference: https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75
- Klavkarr Polo 6R 1.2 TDI CAN 11-bit 500kbps: https://www.klavkarr.com/it/compatible/volkswagen-polov-6r-60000177
- SAE J1962, geometria e interfacce del connettore diagnostico (verificare Type A e datasheet del connettore scelto): https://law.resource.org/pub/us/cfr/ibr/005/sae.j1962.2002.pdf
- JLCPCB PCBA capabilities: https://jlcpcb.com/capabilities/pcb-assembly-capabilities
- JLCPCB custom post-assembly services: https://jlcpcb.com/help/article/jlcpcb-supported-personalized-services
- ADI CAN termination considerations: https://www.analog.com/en/resources/app-notes/an-1123.html
- ESP-IDF security: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/security.html

**Questi riferimenti non equivalgono ai datasheet di ogni componente effettivo né alla certificazione del prodotto.**
