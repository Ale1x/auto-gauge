# HW-04 — studio indipendente del sensing USB / OBD

24 ottobre 2026. **H0.1, schema di studio NON qualificato e NON collegato ad A0.** Nessun layout, ordine o prova veicolo. L'ERC verifica la connettività, non completa i requisiti elettrici.

## Risultato concreto

Creato `presence_sensing_study.kicad_sch`, con cache locale `PresenceStudy.kicad_sym`, progetto, netlist XML, BOM/pin matrix, ERC JSON e render PNG. U1 è il candidato **TLV7042QDGKRQ1**, dual comparatore open-drain alimentato dal medesimo 3,3 V commutato dell'ESP32. Due partitori indipendenti osservano USB raw e AON_PROTECTED. Un riferimento comune ricavato dal 3,3 V imposta soglie ratiometriche. Le due uscite hanno pull-up esclusivamente al 3,3 V commutato: nessuna resistenza o diodo le collega direttamente alle sorgenti USB/OBD.

Il datasheet TI §7.4.1 dichiara ingressi fault-tolerant e isolamento da VCC anche con VCC=0, fino al limite d'ingresso 7 V. È una base esplicita per evitare il clamp verso il rail spento. **Non fornisce però massimi IB/ILKG, corrente con VCC=0 o isolamento quantitativo fra ingressi in tutte le condizioni**. Pertanto non è dimostrato un massimo di backpower/leakage né il consumo totale a caldo. Anche il solo ramo USB resta H per queste ragioni: non confondere la proprietà funzionale fail-safe con un leakage massimo misurabile garantito.

## Topologie confrontate

| Topologia | Pregio | Problema verificato / esito |
|---|---|---|
| NMOS 2N7002BKS, source GND, drain con pull-up 3V3, gate da sorgente o partitore | Nessuna dipendenza da beta BJT, nessun clamp del gate verso 3V3 | Datasheet Nexperia 17-10-2024 tabella7: IGSS max10µA a25°C/20V; IDSS max1µA a25°C e10µA a150°C/60V; VGSth1,1–2,1V è definito a250µA, non una soglia logica garantita. RDSon max è dato a5/10V, non alla tensione ridotta del gate. Un gate leak di10µA supera oltre12 volte il consumo del partitore OBD scelto e produce circa9,3V su Rth≈0,93MΩ: nessun margine dimostrato con resistenze in MΩ. Scartata come soluzione dimostrata a bassissimo consumo; non si assume che il massimo a20V sia garantito a ogni tensione/temperatura. |
| TLV7042-Q1 fail-safe, partitori e riferimento da3V3, open-drain | Soglie calcolabili, ingressi esplicitamente indipendenti da VCC; uscite nello stesso dominio MCU | Candidato disegnato. IB2pA e leakage uscita100pA sono **tipici**, non massimi; ICC750nA/canale max è specificato aVS1,8V, non automaticamente a3,3V. Restano aperti massimi full-temperature/off-state e VOL a3,0V. |

Un BJT con guadagno presunto a correnti di base microampere non è usato né considerato dimostrazione. Non è stato selezionato un logic-Ioff generico senza verifica delle sue tensioni ammesse con VCC=0.

## Inviluppi H e valori dello schema

- Alimentazione valida H3,0–3,6V; temperatura di screening −40..125°C. Il range di alimentazione raccomandato del comparatore è1,6–6,5V, ma i risultati logici qui sono utilizzabili solo quando rail MCU e sensing sono qualificati. Non è un nuovo requisito prodotto.
- USB raw: H0..5,5V positivo, sorgente non-PD. R1=5,1MΩ, R2=1MΩ. Non autorizza PD, sovratensioni, transitori negativi o corrente USB. CC/protezioni rimangono nel filone alimentazione.
- AON_PROTECTED: **solo H0..40V positivo per lo studio**, R3+R4=6,8MΩ+6,8MΩ, R5=1MΩ. Non è una decisione sul limite automotive reale: TVS/clamp/fault/reverse/sleep/wake A0 non sono chiusi. Non collegare a OBD raw. Il front-end deve garantire questo inviluppo al nodo; oltre40V o sottoGND serve rifare analisi/protezioni.
- R6=5,1MΩ / R7=1MΩ da3V3: riferimento nominale0,541V a3,3V. R8/R9=10kΩ; C1=100nF16VX7R vicinoVCC. Assenza di condensatori sui nodi MΩ intenzionale: capacità/leakage/settling e contaminazione devono essere dimensionati e verificati prima dell'integrazione.
- Resistenze1%,100ppm/°C: screening conservativo±2% per100°C da25°C, senza aggiungere una promessa sull'invecchiamento. Sorgenti Yageo confermano75V continuous a full rated power e0,1W@70°C. Ogni6,8M vede meno18,69V e meno53µW nel casoH40V considerato. Rating DC non è rating impulsivo automotive.

Il partitore OBD tiene l'ingresso sotto2,844V a40V e tolleranze±2%, quindi margine di common-mode **0,256V** rispetto3,0+0,1V (senza IB). Con H|IB|100nA, massimo2,939V. Gli ingressi non vanno alimentati oltre il loro common-mode per pretendere una misura valida: la tolleranza7V è sopravvivenza/fail-safe, non range funzionale.

## Soglie: numeri riproducibili, non garanzie

`threshold_sensitivity.csv` enumera estremi indipendenti di rail/resistenze, offset±8mV e metà isteresi massima25mV. Usa tre scenari IB: zero, ±1nA e ±100nA. La corrente dei due ingressi di riferimento può sommarsi. Il datasheet non dà il massimo IB: nessuna riga va interpretata come qualifica full-temperature. Offset/isteresi sono ai punti di prova indicati nella tabella TI; eventuale dipendenza da common-mode è un limite ulteriore.

| Scenario H | USB transizione possibile | OBD transizione possibile |
|---|---:|---:|
| IB=0 |2,685–3,978V|6,401–9,558V|
| IB assoluto≤1nA ipotizzato |2,670–3,995V|6,365–9,598V|
| IB assoluto≤100nA ipotizzato |1,212–5,568V|2,749–13,513V|

Con lo scenario1nA, USB4,5V e OBD12V sono distinti da0V con margine. Con100nA non sono più garantiti gli stessi stati: quantifica il motivo per cui serve un massimo. AON6V può risultare assente; è accettabile solo come semantica H della misura, non è stato chiesto/deciso dal proprietario. L'uscita indica tensione sopra soglia, **non connettore fisicamente inserito e non IGN**. Non usare il sensing per autorizzare TX CAN prima della qualificazione delle guardie.

## Correnti e partial power

Il partitore OBD assorbe0,822µA nominali a12V e **0,839µA massimo per le sole resistenze** nello screening±2%. Non è il massimo dell'intero sensing. Quello resta **non determinabile dal datasheet** senza massimi di input/off-state leakage. A3,3V spenti il riferimento e i pull-up non hanno una sorgente attiva; il comparatore non viene però contato artificialmente come zero leakage. Non è dimostrato il target totale≤100µA a12V entro5min.

Con3V3 attivo, il riferimento aggiunge≤0,603µA e ogni uscita LOW può aggiungere≤367,4µA a3,6V: entrambiLOW≤734,7µA dal rail3V3, oltreICC e leakage. Il divisore USB assorbe≤0,921µA a5,5V. Per riferire i carichi3V3 a12V servono efficienze/regimi reali delle due conversioni, non una semplice somma di correnti. ICCmax750nA/canale al solo puntoVS1,8V non è usato come budget massimo a3V3.

I pull-up10k consentirebbero una somma leakage uscita+GPIO fino circa73,5µA prima di violare VIH=0,75VDD a3V0 (screening GPIO50nA dal datasheet Espressif a25°C). VOLmax350mV TI è aVS5V/I3mA; non lo si dichiara garantito aVS3V0, anche se il carico è molto più basso. La tabella GPIO Espressif è a25°C: la verifica full-temperature resta aperta.

| Sorgenti (rail3V3 valido) | USB_PRESENT_H / OBD_PRESENT_H attesi* |
|---|---|
| USB4,5–5,5V soltanto |HIGH / LOW|
| AON12V soltanto |LOW / HIGH|
| entrambe |HIGH / HIGH|
| entrambe0V, ma3V3 mantenuto da bench |LOW / LOW|
|3V3 spento, qualsiasi sorgente ammessa |Nessuna lettura logica valida; ingressi fail-safe, pull-up senza sorgente|

*Attesi sotto le assunzioni H1nA e dopo qualificazione/stabilizzazione; non risultati di misura. Nel prodotto entrambeOFF normalmente implica anche3V3OFF. Nessuna uscita va alimentata esternamente. Configurare GPIO38/39 come input; evitare pull-up a rail diversi. Le due sorgenti hanno soltanto il ritornoGND comune; il circuito non è isolamento galvanico. L'isolamento VCC dichiarato evita il percorso di clamp diretto, ma non certifica una corrente fra domini nulla o numericamente limitata in ogni stato.

La partenza del comparatore è200µs **tipica**: non fissare un timer garantito da questo dato. Gli stati con rail basso/rampante, residuale sui partitori o transizioni rapide devono restare invalidi finché la qualificazione non è definita e verificata. Sensing non determina se un telefono è collegato e non concede corrente USB.

## Fonti e disponibilità

Verifica documentale24-10-2026. Stock mostrato dai risultati consultati, nessuna prenotazione; può cambiare. Metadati di distributore non sostituiscono tabelle TI: per esempio “bias max” e “delay max” sono talvolta etichette errate per valori tipici.

| MPN candidato | Fonte primaria / stock consultato |
|---|---|
| TLV7042QDGKRQ1 | [TI datasheet SNOSDA5D,§5.2/6.9/6.10/7.4.1](https://www.ti.com/lit/ds/symlink/tlv7031-q1.pdf), salvato in sources; [DigiKey](https://www.digikey.com/en/products/detail/texas-instruments/TLV7042QDGKRQ1/15781488)3.256pezzi mostrati |
| RC0603FR-075M1L | [Yageo specsheet](https://yageogroup.com/component-documentation/download/specsheet/RC0603FR-075M1L), salvato; [DigiKey](https://www.digikey.com/en/products/detail/yageo/RC0603FR-075M1L/5918378)23.144pezzi mostrati |
| RC0603FR-076M8L | [Yageo specsheet](https://yageogroup.com/component-documentation/download/specsheet/RC0603FR-076M8L), salvato; [Newark cut tape](https://www.newark.com/yageo/rc0603fr-076m8l/res-thick-film-smd-6m8-100mw-1/dp/98K7432)810pezzi mostrati; listing segnala50V, usare75V solo da fonte primaria e chiarire discrepanza prima acquisto |
| RC0603FR-071ML | [Yageo specsheet](https://yageogroup.com/component-documentation/download/specsheet/RC0603FR-071ML), salvato; [Newark](https://www.newark.com/yageo/rc0603fr-071ml/res-1m-1-0-1w-0603-thick-film/dp/98K7342)330.321pezzi mostrati |
| RC0603FR-0710KL | [Yageo specsheet](https://yageogroup.com/component-documentation/download/specsheet/RC0603FR-0710KL), salvato; [Mouser](https://www.mouser.com/en/ProductDetail/603-RC0603FR-0710KL)2.782.034pezzi mostrati |
| GRM188R71C104KA01D | [Murata SimSurfing](https://ds.murata.co.jp/simsurfing/ceramic_capacitor.html), caratterizzazione specifica e lifecycle ancora da verificare; [LCSC](https://www.lcsc.com/product-detail/Multilayer-Ceramic-Capacitors-MLCC-SMD-SMT_Murata-Electronics_C45000.html)63.450pezzi mostrati. CandidatoH, non congelato. |
|2N7002BKS (confronto scartato)|[Nexperia datasheet](https://assets.nexperia.com/documents/data-sheet/2N7002BKS.pdf), tabella7 verificata via web primario. [DigiKey Nexperia](https://www.digikey.com/en/products/detail/nexperia-usa-inc/2N7002BKS-115/2531264)63.591pezzi mostrati. Download locale restituisce HTML: conservato come `2N7002BKS_download_error.html`, non spacciato per PDF.|

## Verifica e prossime condizioni di integrazione

Eseguire `python3 scripts/design_presence_sensing.py` dalla root. Scrive solo questa directory, importa gli helper di lettura/generazione senza eseguire il loro main, esegue il vero KiCad CLI, confronta tutte le assegnazioni pin reali nella netlist e controlla i nodi chiave. ERC JSON: **0 rilievi**,35pin assegnati verificati; i PWR_FLAG dichiarano soltanto la sorgente3V3 e il ritorno del banco esterno. Pin U1 verificati contro tabella5-2:1OUTA,2INA−,3INA+,4VEE,5INB+,6INB−,7OUTB,8VCC. Cache/progetto solo locali. Render controllato visivamente.

Prima di sostituire i placeholder A0 servono: massimi documentati di leakage/offstate o componente alternativo con limiti garantiti; conferma soglie/VOH/VOL full-temperature; inviluppo AON e protezione transitori con energia; budget completo12V; validità startup/fault e controllo leakage PCB. L'apertura di una resistenza non è stata qualificata come single-fault: per esempio R5 aperta può esporre INB a tensione eccessiva. Nessun gate hardware è dichiarato superato; nessuna domanda prodotto è necessaria per consegnare questo incremento.
