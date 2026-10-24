# HW-03 — preregolatore USB H3,792 V

Revisione di studio H0.1, 24 ottobre 2026. **Circuito separato da A0, non costruibile e non equivalente a una USB completa.** Non sono stati modificati A0, la sua distinta o le sue segnalazioni. I nuovi valori e MPN sono candidati, non scelte approvate per acquisto.

## Nuovo risultato concreto

`usb_preregulator_study.kicad_pro` apre un singolo foglio con 14 simboli: TPS62160-Q1, induttore, quattro condensatori, partitore, bias EN/PG, due interfacce di banco e dichiarazioni della sorgente esterna. Progetto inizializzato tramite MCP, poi generato in modo deterministico. La libreria standard è inclusa nella cache del foglio; non serve modificare librerie di sistema.

Netlist reale in `study.net.xml`; report KiCad CLI 10.0.7 in `erc.json`: **0 segnalazioni ERC**. Sono verificati i collegamenti pin/net, incluso VOS sull’uscita dopo l’induttore, FB sul partitore e tutti i ritorni del regolatore. Lo zero ERC riguarda soltanto questo blocco logico con sorgente esterna dichiarata: non misura stabilità, tensione, protezioni, corrente USB o idoneità dei footprint.

Il progetto non è collegato al TPS2116 o al TPS62811 reali. In particolare, J1 non è una USB-C: rappresenta la sorgente di banco **a valle di protezioni ancora mancanti**. EN parte spento. Il foglio riporta questi OPEN in evidenza.

## Scelta della tensione: perché studiare 3,8 V

Il partitore candidato è 37,4 kΩ/10 kΩ, quindi VOUT nominale = 0,8 × (1+3,74) = **3,792 V**. Il confronto usa valori E96, non tensioni ideali arrotondate:

| Obiettivo H | Rtop/Rbottom | Nominale reale | Finestra condizionata | Minimo dopo mux H | Margine del secondo buck H | RHS massimo ammesso al primo buck H |
|---|---|---:|---:|---:|---:|---:|
| 3,8 V | 37,4k/10k |3,792 V|3,643–3,981 V|3,608 V|78 mV|0,713 Ω|
| 4,0 V | 40,2k/10k |4,016 V|3,858–4,216 V|3,824 V|294 mV|0,332 Ω|
| 4,2 V | 42,2k/10k |4,176 V|4,011–4,385 V|3,979 V|449 mV|0,031 Ω|
| 4,5 V | 46,4k/10k |4,512 V|4,333–4,738 V|4,303 V|773 mV|negativo|

La tensione più bassa migliora il margine del primo buck a VBUS basso, ma riduce quello del secondo. In questa fase **3,8 V è l’opzione di studio**, non un nuovo requisito di prodotto. Tutte le righe restano `CONDITIONAL_SCREEN_NOT_GUARANTEED`.

### Condizioni del confronto

- VBUS al dispositivo = 4,5 V H, prima della protezione USB; carico 3,3 V = 0,5 A H.
- Per non ignorare il nominale A0 di circa 3,315 V e le tolleranze, si studia un’uscita superiore H di 3,42 V. Non è un nuovo limite approvato.
- Accuratezza FB del TPS62160 −3/+4%, leakage FB ±400 nA; resistori ±0,1% iniziali più sensibilità ±0,25% ciascuno per un’escursione H di 100 °C e TCR25 ppm/°C. Non sono inclusi invecchiamento/umidità né un profilo termico definitivo.
- Efficienze H80% per ciascuno stadio, RON mux 60 mΩ e protezione d’ingresso 131 mΩ usate come sensibilità. I valori di RON hanno condizioni di tabella proprie; non sono automaticamente un bound a ogni corrente e tensione.
- Per il TPS62811 si ipotizzano RHS120 mΩ e DCR dell’induttore100 mΩ: VIN richiesto H = 3,42 + 0,5 × (0,12+0,10) = **3,53 V**. La tabella del componente fornisce RHS massimo60 mΩ aVIN≥5V, non al rail qui studiato. Quindi i120 mΩ sono uno screening, non un limite garantito dal produttore.

Il dato più utile da qualificare è ora esplicito: per la riga3,8 V il primo buck deve avere RHS ≤0,713 Ω alle condizioni USB basse considerate; per il secondo la somma RHS+DCR deve restare entro circa0,376 Ω al punto conservativo calcolato, prima del margine per transitori. Il TPS62160 dichiara600 mΩ massimi soltanto aVIN≥6V; non si può estenderli a4,5V senza ulteriore evidenza. Sono necessarie curve qualificate o verifica al banco/TI alle temperature effettive.

Tutte le finestre statiche restano nell’intervallo d’ingresso del TPS2116 (1,6–5,5 V) e del TPS62811 (2,75–6 V); essere sopra2,75 V non garantisce però l’uscita3,3 V a0,5 A. `rail_tradeoff.csv` conserva formule e risultati.

## Componenti dimensionati come candidati

| Riferimento | MPN candidato | Dato usato e stato di approvvigionamento |
|---|---|---|
| U1 | TPS62160QDSGRQ1 | 3–17 V,1 A, nominalmente2,25 MHz, duty100%; già verificato in HW-02. Footprint Q1 non congelato. |
| L1 | XAL4030-332MEC | 3,3 µH ±20%, DCR massimo28,6 mΩ a25 °C; Isat tipico5,9 A per30% calo. Il [produttore](https://www.coilcraft.com/en-us/products/power/shielded-inductors/molded-inductor/xal/xal40xx/xal4030-332/) pubblica dati e curve. [DigiKey Marketplace](https://www.digikey.ca/en/products/detail/coilcraft/XAL4030-332MEC/21381756) e LCSC indicano indisponibilità nei canali esaminati: **sourcing aperto**, nessuna disponibilità inventata. |
| C1/C2 | GRM31CR71E106KA12L, due pezzi | 10 µF ±10%,25 V,X7R,1206. [Murata SimSurfing](https://ds.murata.com/simsurfing/mlcc.html?oripartnumbers=%5B%22GRM31CR71E106KA12L%22%5D&partnumbers=%5B%22GRM31CR71E106KA12%22%5D) identifica il modello e le curve. [DigiKey](https://www.digikey.com/en/products/detail/murata-electronics/GRM31CR71E106KA12L/2345330) indicizzava0 pezzi: disponibilità da riconfermare o candidato da sostituire. |
| C3/C4 | CGA6P1X7R1C226M250AC, due pezzi | 22 µF ±20%,16 V,X7R,1210,AEC-Q200. [TDK](https://product.tdk.com/en/search/capacitor/ceramic/mlcc/info?part_no=CGA6P1X7R1C226M250AC) conferma rating e produzione. [DigiKey](https://www.digikey.com/en/products/detail/tdk/CGA6P1X7R1C226M250AC/2443201), pagina aperta:31525 pezzi, USD1,24 qty1 al momento della lettura. |
| R1 | AT0603BRD0737K4L | 37,4 kΩ,0,1%,25 ppm/°C,0603. Catalogo Mouser indicizzato:1093 pezzi, USD0,31 qty1; osservazione commerciale, non quotazione. |
| R2/R3/R4 | AT0603BRD0710KL | 10 kΩ,0,1%,25 ppm/°C,0603. LCSC indicizzava680 pezzi con MOQ10; DigiKey standard0, stock marketplace distinto. Nessuna equivalenza fra stock e disponibilità JLCPCB. |

Tutte le osservazioni sono del24/10/2026 e non riservate. Le schede tecniche dei resistori sono documenti originali Yageo, disponibili anche presso [TTI](https://www.tti.com/content/dam/ttiinc/manufacturers/yageo/doc/yageo-at-anti-sulfurated-chip-resistor-datasheet-specification.pdf). I case standard assegnati aR/C sono ipotesi di footprint, non DFM approvata. Connettori e power flag sono esclusi dalla distinta di acquisto e dalla scheda.

### Induttore e condensatori effettivi

Si usa3,3 µH perché il [datasheet TPS62160-Q1](https://www.ti.com/lit/ds/symlink/tps62160-q1.pdf), §9.2.2.2, lo raccomanda a basso VIN; la tabella LC comprende3,3 µH con22/47 µF nominali. La curva Coilcraft L-corrente è stata scaricata e ispezionata: `sources/coilcraft_xal4030.pdf`, pagina2. Non è un inviluppo minimo garantito su temperatura. Il modello assume un ulteriore calo H10% oltre la tolleranza iniziale, Lmin H =2,376 µH. AFSW H1,5 MHz eVIN5,5V si ottengono circa0,330 A di ripple e0,752 A di picco; il confronto con5,9 A a25 °C non sostituisce il controllo a caldo e in fault. DCR a125 °C è stimata con coefficiente del rame0,00393/K:39,84 mΩ H. Non è una curva garantita di DCR del componente montato.

Per gli MLCC **non è stato ottenuto un inviluppo numerico garantito delle curve bias**. La scheda TDK con curve è stata identificata sul sito originale; il download locale restituisce HTTP403. L’estrazione consultabile mostra grafici bias/frequenza/temperatura, ma non autorizza a inventare punti o minimi. Il modello usa pertanto fattore bias H0,70, temperatura H0,85, tolleranza0,90 per input e0,80 per output. Ne risultano Cinput effettiva H10,71 µF e Coutput H20,944 µF. L’invecchiamento non è incluso. Due22 µF nominali non significano44 µF effettivi.

Con quei valori H la frequenza LC è circa22,56 kHz; non è un’analisi di margine di fase. Prima del congelamento servono curve numeriche del lotto/famiglia, assieme alle effettive condizioni di bias/ripple e alla verifica della matrice LC del produttore. Anche il secondo buck richiede Ceff e L effettivi verificati: questa revisione non dimensiona i passivi A0 di quel rail.

## Enable, power-good, avvio e corrente USB

EN è riportato aGND da10 kΩ e lasciato su un ingresso esterno `ENABLE_QUALIFIED_OPEN`: **l’assenza della logica qualificata mantiene il blocco spento**. Prima di abilitarlo servono condizioni di ingresso/corrente valide. Non collegare EN a una sorgente indipendente che possa superare VIN quando il ramo è spento.

PG è open-drain, con pull-up10 kΩ verso l’uscita H3,792 V, e termina all’interfaccia di test. Non è collegato al MCU3,3 V: quel livello non è automaticamente GPIO-safe. Le sogliePG pubblicate sono relative aVOUT e non certificano la disponibilità del rail3,3 V né l’assenza di corrente inversa. Un futuro gate del TPS62811 deve anche funzionare con il ramo OBD; non è stato implementato un OR di enable implicito.

Il soft-start interno è circa25 mV/µs **tipici**, dopo circa50 µs:44 µF nominali richiedono1,1 A di sola carica all’uscita a quello slew; con capacità H massima60,72 µF, la stima sale a1,518 A. La limitazione interna può cambiare la rampa e la termica. Non esiste qui un controllo esterno del tempo di soft-start. Occorre co-progettare la rampa upstream, la corrente USB ammessa e l’abilitazione dei carichi a valle: i soli condensatori non garantiscono un ingresso entro500 mA.

Al punto conservativo del confronto,0,5 A sul3,3 V e80%×80% richiedono **0,594 A USB di solo carico**, prima diIQ, partitori e carica: non entra in500 mA. Questo è un conflitto numerico del punto di studio, non un nuovo limite di prodotto né una prova che la recovery ROM consumi quella corrente. La capacità di fornire picchi al MCU, il carico ROM reale e la corrente consentita daCC/enumerazione vanno misurati/qualificati separatamente. Il preregolatore non attribuisce1,5A/3A alla porta.

## Fault e corrente inversa ancora aperti

Il TPS62160 non è una OVP certificata per il mux: §9.2.2.1 dichiara un clamp circa7,4 V alVOS in caso diFB aperto, superiore all’absmax6 V delTPS2116. Il funzionamento normale a3,792 V non copre tale guasto. Non è dichiarata una protezione single-fault completa; occorre decidere l’architettura di protezione/fault analysis prima di integrare.

Nessun EN basso è assunto come isolamento bidirezionale. Con OBD alimentata, il mux dovrebbe isolare l’uscita del buck USB, ma leakage e transitori del sistema, alimentazione dei pinPG/EN, sorgente USB a0V con cavo inserito e backfeed verso VIN richiedono verifica. Il presente circuito ha solamente la porta di uscita di studio; nessuna corrente inversa è stata simulata o misurata.

OVP/corrente/inrush a monte, CC/ESD, corrente ROM, protezione output da fault e rilascio brownout rimangono OPEN espliciti. Il rating25 V dei condensatori di ingresso non estende il regolatore oltre17 V; non viene negoziato PD né ammessa alimentazione nominale24 V.

## Reset USB fisico: dipendenze hardware, non nuovo gesto

DEC-011 consente N client BLE in sequenza con una connessione attiva; il cambio telefono non richiede reset. Il reset fisico USB/pulsanti rimane manutenzione locale separata: credenziali/configurazione/log del gauge, senza cancellazione degli storici cloud. Non sono definiti qui gesto, durata o combinazione di pulsanti.

Nello schema A0 SW701 agisce suEN/RESET e SW702 suGPIO0/BOOT: sono candidati di accesso recovery, non una sequenza di factory reset già implementata. USB_PRESENT_SAFE_TBD non ha ancora il circuito di sensing, per cui la presenzaUSB non può essere assunta valida da un GPIO aperto. Devono funzionare il rail3,3 V e il rilevamento alimentazione anche inUSB-only; il reset deve essere distinguibile da un semplice brownout e la cancellazione locale deve reggere interruzioni di alimentazione. Non è stata realizzata tale logica, modificata laHAL o programmata alcuna eFuse.

## Riproduzione e limite del deliverable

Eseguire `python3 scripts/design_usb_preregulator.py`: rigenera confronto tensioni, dimensionamento H, schema standalone, distinta candidata, matrice pin, SVG, netlist, ERC completo e manifest. Lo script legge solo le funzioni di formato/libreria di `scripts/generate_review_schematic.py` e ne registra l’hash; non chiama il generatore A0 né le sue routine di salvataggio.

Lo schema è stato inizializzato e visualizzato mediante MCP. Il controllo finale usa il JSON gerarchico della CLI, così non dipende dal riepilogo ERC incompleto del server. È stata verificata la visualizzazione del foglio; non sono state eseguite simulazione SPICE, prove di banco o review dei land pattern.

Il prossimo passo tecnico è circoscritto: ottenere il bound RHS aVIN basso di entrambi gli stadi e le curveCeff numeriche, sostituire i candidati senza stock, quindi simulare la rampa con protezione/current budget reale. Nessuna richiesta di approvazione prodotto necessaria per continuare queste verifiche.
