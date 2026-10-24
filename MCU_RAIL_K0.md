# Rail MCU 3,3 V — verifica K0, 2026-10-24

Studio candidato; nessuno schema definitivo o requisito aggiuntivo approvato. Il pannello e i suoi rail restano separatamente da dimensionare. Requisiti correnti v0.2.5, DEC-001–008.

## Corrente e confronto termico

E02 §6.2 richiede una sorgente esterna capace di almeno0,5A per il modulo a3,3V. È capacità richiesta alla sorgente, non consumo costante misurato e non valore massimo di tutto il gauge. La tabella BLE riporta picchi dipendenti dalla potenza radio, non il budget completo di UI, PSRAM, flash e periferiche. Il Wi-Fi resta escluso dallo scope.

TPS7A21-Q1 (E47), LDO500mA, raggiunge tale valore senza margine nominale per altri carichi sullo stesso rail. A5→3,3V e0,5A, perdita ideale0,85W prima della corrente propria del regolatore. TPS7A2133PQWDRBRQ1 è un esempio3,3V documentato, non un componente selezionato: non viene adottato come rail unico MCU/periferiche. Non è dimostrato che qualunque LDO sia impossibile; serve confronto termico sul layout reale.

Modello riproducibile: scripts/build_mcu_rail_tradeoff.py → evidence/models/mcu_rail_tradeoff.csv. Perdita LDO=(5−3,3)I; perdita buck=P_out(1/η−1). Efficienze80/85/90% sono ipotesi, non curve del componente.

| Carico 3,3V | Perdita LDO ideale | Perdita buck con η90% ipotetica | Corrente da5V con η90% |
|---:|---:|---:|---:|
| 0,10A | 0,170W | 0,037W | 0,073A |
| 0,20A | 0,340W | 0,073W | 0,147A |
| 0,35A | 0,595W | 0,128W | 0,257A |
| 0,50A | 0,850W | 0,183W | 0,367A |

La differenza a0,5A è circa0,667W nelle ipotesi. Il buck aggiunge induttore, condensatori, layout critico e rumore da verificare su CAN/ALS/BLE/display. I parametri termici JEDEC non predicono da soli la temperatura nel case; nessuna temperatura interna dichiarata.

## Candidato buck da approfondire

**TPS62811QWRWYRQ1**, famiglia TPS6281x-Q1,1A, ingresso2,75–6V e uscita regolabile (E46). È un secondo stadio da un dominio5V protetto e commutabile, mai da B+ diretto. L’ingresso5V e il TPS2116 devono restare entro i rispettivi limiti anche durante overshoot, guasti e hotplug: il componente automotive non sostituisce la protezione del sistema.

Datasheet Rev.K, tabella varianti: uscita regolabile, output discharge ON, spread spectrum OFF. Non trasferire opzioni da un suffisso diverso: TPS628110A è1,2V fisso e non è un sostituto3,3V; TPS6281120 differisce per spread spectrum. Il suggerimento «equivalente» del distributore non congela una variante.

PackageRWY9pin3×2mm con wettable flanks. Il datasheet richiede per la versione1A capacità effettiva d’uscita minima15µF e d’ingresso5µF, prima di ulteriori vincoli di compensazione. Valore nominale stampato e capacità effettiva sotto bias non sono intercambiabili. Induttanza, resistenze, frequenza, EN, soft-start e PGOOD restano da progettare; nessun valore applicativo congelato o footprint approvato.

Shutdown: tabella §6.5 indica1,5µA tipici a25°C e26µA massimi a150°C, con18µA nella condizione125°C. Non usare il solo tipico per sostenere100µA di sistema. Se il dominio5V viene realmente spento, non aggiungere semplicemente26µA a12V: mappare alimentazioni residue, leakage USB/GPIO e percorsi di ritorno del circuito reale. Output discharge non prova isolamento inverso del rail.

E48: DigiKey espone disponibilità di decine di pezzi e prezzi indicativi diversi fra risultati (circaUSD2,12–2,47 a quantità1); confermare pagina/lotto/prezzoUE e disponibilitàJLC prima del freeze. Non è stock riservato. Il TPS62813 da3A è stato esaminato ma nel canale consultato risulta senza stock; nessun upgrade automatico a3A.

## Recovery USB: bilancio separato

Con0,5A a3,3V e η90% ipotetica, il solo rail MCU richiede circa0,367A da5V o0,386A da4,75V, prima di mux, CC, ESD, flash/periferiche esterne e inrush. Il dimensionamento della sorgente MCU non autorizza automaticamente ad assorbire tale corrente dalla portaUSB.

Il servizio ridotto con display/CAN/log spenti è un’ipotesi coerente con la recovery, da provare anche con applicazione corrotta e ROM downloader. Il riconoscimento della corrente disponibile tramiteCC e gli stati prima/dopo configurazioneUSB vanno progettati: non basta una presa con rating elevato o il solo inserimento diRd. TUSB320 (E51) è consultato come esempio documentale di rilevamento correnteTypeC, non selezionato per la BOM. Non è stato introdotto USB-PD o un caricatore obbligatorio.

La recovery firmata e la politica eFuse sono trattate in USB_RECOVERY_K0.md: non assumere la modalitàROM DFU disponibile dopoSecureBoot. Nessun percorso può dipendere solo da firmware applicativo funzionante se deve recuperare un’immagine corrotta.

## Prove e dati mancanti

Prima del freeze: budget di tutti i carichi sul3,3V, transienti MCU/flash/BLE con Wi-Fi escluso, ripple, overshoot, inrush, soft-start, efficienza e temperatura al massimo carico e alimentazioneUSB/OBD. Verificare enable con MCU spento, brownout, spegnimento periferiche e backfeed attraversoGPIO. Ripetere con erase log eOTA. O-05/O-07/O-15 e verifica diDEC-007 restano aperti; nessuna build o misura hardware eseguita.
