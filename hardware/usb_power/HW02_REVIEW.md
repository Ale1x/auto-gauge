# HW-02 — alimentazione USB e recupero fault/wake

24 ottobre 2026. Studio separato A0, con calcoli riproducibili in `scripts/analyze_usb_power.py`. **HR-01 e HR-02 restano aperti.** Nessun collegamento modificato nello schema A0, nessun PCB e nessun componente congelato. Tutte le reti resistive, capacità, correnti di carico ed efficienze proposte qui sono ipotesi H, salvo limiti esplicitamente attribuiti al datasheet.

## Esito operativo

1. **Respinte come soluzione completa le due protezioni OVP a soglia direttamente davanti al TPS2116:** non dimostrano un limite dinamico di5,5V; il caso USB alto elimina il margine statico. La variante clamp5,7V è respinta anche staticamente.
2. **Da sviluppare l’alternativa con preregolatore USB da circa4,2–4,5V**, poi ingresso2 del mux e buck3,3V esistente. Aumenta molto il margine verso5,5V, ma dropout all’ingresso minimo, avvio, corrente USB e transitori restano da qualificare. Non è un nuovo rail approvato dal proprietario.
3. Per HR-01, sostituire il controller con LM74910H-Q1 elimina il solo latch da sovracorrente SLEEP; **non** risolve automaticamente la termica, i picchi di retry o tutti i fault. Un controller a basso IQ sempre attivo come LM74721-Q1 evita la dipendenza dal bypass SLEEP, richiedendo di riprogettare la protezione di corrente. Un ramo ausiliario LDO/timer è una terza possibilità, più complessa e ancora incompleta.

Non è stato creato un circuito KiCad separato: in questa iterazione l’assenza dei margini dinamici impedisce di fornire uno schema da interpretare come protezione completata. Gli artefatti analitici documentano precisamente le opzioni respinte e quella da approfondire.

## Due protezioni reali e approvvigionamento

| Candidato | Funzioni rilevanti dal produttore | Limiti rilevanti | Disponibilità osservata, non riservata |
|---|---|---|---|
| **TPS259631DDAR** | OVP cutoff regolabile, corrente regolabile, slew-rate; auto-retry termico | VIN2,7–19V; ILIM0,125–2A; OVLO1,17–1,22V; tempo OVP1,3µs **tipico**; reverse blocking non specificato | [DigiKey](https://www.digikey.com/en/products/detail/texas-instruments/TPS259631DDAR/10445330), pagina aperta e stock letto:1162, MOQ1 cut tape, USD1,00 qty1 |
| **TPS25940AQRVCRQ1** | OVP/UVLO regolabili, limite corrente e slew-rate, FET back-to-back e reverse blocking, auto-retry termico | VIN2,7–18V; ILIM0,6–5,3A; OVP0,97–1,01V; ritardo OVP2µs **tipico** | [DigiKey](https://www.digikey.com/en/products/detail/texas-instruments/TPS25940AQRVCRQ1/6100244), pagina aperta e stock letto:9476, MOQ1 cut tape, USD3,08 qty1 |

Verifica effettuata il24/10/2026; il numero mostrato può cambiare anche fra due letture. Nessun preventivo, prenotazione, verifica JLCPCB o costo consegnato in Italia. Le pagine TI dichiarano entrambi ACTIVE; lo stock del catalogo del distributore non è un’approvazione elettrica. Il ramo USB temporaneo non assume automaticamente un rating ambientale diverso da quello del dispositivo completo.

Fonti tecniche: [TPS2596 Rev.A](https://www.ti.com/lit/ds/symlink/tps2596.pdf), sezioni7.5/7.6/8.3; [TPS25940-Q1 Rev.E](https://www.ti.com/lit/ds/symlink/tps25940-q1.pdf), sezioni7.5/7.6/8.3. I PDF locali e i relativi hash sono in `sources/` e `source_manifest.json`. Le tabelle6/7 sono state anche renderizzate e controllate visivamente.

## Soglie: una OVP non è un regolatore

Il TPS2116 raccomanda ingressi1,6–5,5V; il suo absolute maximum6V non è un obiettivo operativo. Nel TPS25962x il clamp selezionato come5,7V ha soglia5,54–5,83V e tensione clamp a10mA fino a5,61V. **Non è una protezione garantita per un ingresso raccomandato massimo5,5V.** Non è stato usato come sostituto del cutoff regolabile.

Per un partitore Rtop/Rbottom, includendo la corrente di leakage del pin:

`VIN_trip = Vref × (1 + Rtop/Rbottom) + I_leak × Rtop`

`ovp_thresholds.csv` esamina i limiti di Vref e leakage±0,1µA, con entrambi i resistori ai loro estremi. I limiti del TPS259631 sono pubblicati nella tabella con condizioni standard VIN12V: il calcolo non è un’estensione indipendente di quella caratterizzazione a ogni forma d’onda USB5V.

| Rete H | Tolleranza resistori H | Trip minimo V | Trip massimo V | Margine statico fino a5,5V |
|---|---:|---:|---:|---:|
| TPS259631, 3,49kΩ/1kΩ | ±1% |5,172|5,564|negativo |
| Stessa rete | ±0,1% |5,245|5,487|13mV, ma può tagliare già sotto5,25V |
| Stessa rete | ±0,01% |5,252|5,479|21mV |
| TPS25940-Q1, 4,43kΩ/1kΩ | ±1% |5,182|5,575|negativo |
| Stessa rete | ±0,1% |5,258|5,494|6,3mV |
| Stessa rete | ±0,01% |5,266|5,486|14,4mV |

Non sono selezionati MPN per questi resistori. Tolleranza iniziale non comprende da sola deriva termica, invecchiamento e contaminazione del PCB. I partitori a bassa impedenza assorbono circa1mA dal ramo USB: non vanno replicati come rami permanenti OBD senza nuovo budget.

Questa finestra5,25→5,5V era soltanto un sottoinsieme di studio. La documentazione primaria [Microchip su Type-C](https://developerhelp.microchip.com/xwiki/bin/view/applications/usb/power-delivery/battery-charging/) riporta vSafe5V fino a5,5V; anche la [specifica Type-C Release1.3 conservata daTI](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/1008/USB-Type_2D00_C-Specification-Release-1.3.pdf), §4.4.2, riporta quel massimo. Non costituisce verifica di conformità all’ultima revisione USB. Se si vuole accettare5,5V alla porta, un cutoff passante direttamente non può contemporaneamente restare sopra5,5V in tutti i corner e impedire al mux di superare5,5V. Il preregolatore evita questa sovrapposizione di finestre.

## Risposta ai fault e ripartenza delle eFuse

Le tabelle dei candidati non danno un massimo del ritardo OVP per costruire una garanzia dinamica. Il valore tipico non è un tempo massimo di apertura del percorso. Nel TPS25940 il ritardo è inoltre definito verso il flag FLT e con overdrive specificato: non equivale a un limite della tensione di uscita.

Come sensibilità, `ΔV = I_excess × Δt / C_eff`:0,5A per1,3µs su22µF danno29,5mV; per2µs danno45,5mV. Sono già maggiori dei margini diretti sopra. La corrente di picco non è limitata istantaneamente al valore ILIM: TPS259631 riporta87µs **tipici** per current limiting. I valori del CSV sono ipotesi illustrative, non una previsione dell’impulso reale. Induttanza del cavo, ringing, ESR, tolleranza delle capacità e risposta FET richiedono modello e prova.

La dicitura auto-retry non elimina l’isteresi OVP. Nelle reti strette studiate, il rilascio cade circa4,85–5,08V per TPS259631 e4,88–5,11V per TPS25940. Un ritorno della sorgente a5V dopo una sovratensione non garantisce che ogni esemplare superi la soglia di rilascio: potrebbe servire rimuovere USB. Non attribuire alla funzione di retry termico una periodicità di ripartenza OVP non dichiarata.

## Corrente, capacità e inrush

Un Rd da5,1kΩ su ciascun CC non conferisce1,5A/3A. Occorre acquisire l’advertisement CC o rispettare i limiti della connessione USB applicabile anche prima dell’enumerazione. Questa analisi non congela un limite USB100/500mA universale, né dichiara conforme il boot ROM su ogni porta.

Per TPS259631, equazione del produttore `ILIM ≈ 0,0112 + 903/RILM[Ω]`. Con2,10kΩ±0,1% H e screening±10,4%, l’intervallo è circa0,395–0,488A, **escludendo** IQ, partitori e transitori. Con7,87kΩ è circa0,113–0,139A: non è un limiter massimo100mA. La condizione pubblicata per il punto125mA è limitata aTA≤80°C; non estenderla ad altre temperature. Il TPS25940, minimo0,6A programmabile, non soddisfa da solo un budget di ingresso500mA o inferiore.

Per TPS259631 si studia Cdvdt47nF±10% H. Usando IDVDT1,89–2,33µA e gain20,31–21,5, lo slew risulta742–1184V/s; salita a5V circa4,22–6,73ms. Con capacità di carico H comprensiva di tolleranza+20%:

| Capacità nominale H | Corrente capacitiva massima di screening |
|---:|---:|
|1µF|1,42mA|
|22µF|31,3mA|
|47µF|66,8mA|
|100µF|142mA|

Questi carichi sono sulla specifica uscita che sta salendo; non si sommano indiscriminatamente capacità su rail diversi. Vanno aggiunti carico attivo e avvio dei convertitori cascati. Il TPS2116 ha la propria rampa: due rampe in cascata non garantiscono la corrente né il tempo calcolati per un solo stadio. Il MCU non può essere assunto spento fino a fine rampa quando EN del buck è direttamente alimentato.

## Terza architettura: preregolatore USB

Proposta di sviluppo: USB VBUS alimenta una protezione d’ingresso con OVP/corrente/inrush, poi un buck a ingresso sufficientemente ampio. Il buck produce un rail H4,2–4,5V verso VIN2 del TPS2116; VIN1 resta5V OBD. L’uscita del mux continua a fornire il TPS62811 che genera3,3V. La protezione USB non viene eliminata dal nuovo buck.

Candidato da simulare: **TPS62160QDSGRQ1**, regolabile3–17V d’ingresso,1A, duty100%; [datasheet Rev.B](https://www.ti.com/lit/ds/symlink/tps62160-q1.pdf) conservato localmente. [Mouser](https://www.mouser.com/en/ProductDetail/Texas-Instruments/TPS62160QDSGRQ1?qs=7z%252BmIopC6%2FJcaztvQ5d0cw%3D%3D) mostrava indicizzati138pezzi e USD2,09 qty1; [DigiKey](https://www.digikey.com/en/products/detail/texas-instruments/TPS62160QDSGRQ1/5080465) indicizzava0pezzi e un’alternativa di confezionamento. La disponibilità del buck è dunque da riconfermare, non uniforme fra canali. Questo candidato non è approvato per acquisto.

Il preregolatore non sopporta arbitrariamente qualsiasi tensione sulla USB:17V di esercizio non sono20V/PD/EPR. Non viene negoziato PD. Una soglia OVP H attorno a6,5V davanti al buck offrirebbe più margine dinamico rispetto a una soglia5,4V davanti al mux, ma il limite dinamico va ancora dimostrato; non si assume che il solo cutoff gestisca uno step20V.

### Finestra di uscita e dropout

Per lo screening si usano riferimento0,8V, accuratezza−3/+4%, leakageFB fino a400nA e partitore±0,1%. Le condizioni delle accuratezze PWM/PSM nel datasheet sono differenti; a basso VIN queste stime **non** garantiscono regolazione. Con Rbottom10kΩ H:

| Rail nominale H | Rtop H | Finestra statica di screening | Margine superiore al mux |
|---:|---:|---:|---:|
|4,2V|42,5kΩ|4,050–4,392V|1,108V|
|4,5V|46,25kΩ|4,339–4,706V|0,794V|

Entrambe le finestre sono compatibili staticamente con VIN2 del TPS2116 (1,6–5,5V) e VIN del TPS62811 (2,75–6V), con margine rispetto a3,3V. Questa maggiore distanza è il motivo per preferire lo studio del preregolatore. Non prova overshoot, dropout sotto carico, EMI o stabilità.

Il TPS62160 può andare al100% duty: `VIN_min = VOUT + IOUT × (RHS + DCRL)`. Il massimo RHS0,6Ω è specificato aVIN≥6V; a3V è dato solo un valore tipico. **Non è un massimo garantito al minimo USB4,5V.** Non si può usare tale dato per promettere la regolazione a4,2V con ogni carico. La variante4,5V ha ancor meno headroom.

Nel modello H con RHS0,6Ω, DCRL0,15Ω e caduta della protezione0,131Ω, VBUS4,5V e conversioni85% ciascuna:

| Carico3,3V H | Corrente USB solo carico | Ingresso richiesto per mantenere4,2V H | Perdita dei due convertitori H |
|---:|---:|---:|---:|
|0,10A|0,1015A|4,283V|0,127W|
|0,20A|0,2030A|4,365V|0,253W|
|0,35A|0,3552A|4,489V|0,444W|
|0,50A|0,5075A|4,613V|0,634W|

A0,5A non entrano né il budget500mA USB né l’headroom ipotizzata del primo buck; perfino a0,35A il margine H è circa11mV prima di tolleranze/overhead. L’uscita del primo buck potrebbe scendere conservando ancora il3,3V, ma occorrono limiti reali e verifica di entrambi gli stadi per dimostrarlo. Il modello non è una curva di efficienza.

IQ del preregolatore:17µA tipici/30µA massimi in condizione non switching; shutdown25µA massimi, non il solo valore tipico1,8µA. Non usare IQ per stimare le perdite al carico: le efficienze H le modellano separatamente. I parametri di RON sono anch’essi legati alle condizioni di tabella; il loro uso qui è uno screening, non una garanzia estesa.

### Corrente inversa e alimentazione parziale

| Stato | Verifica necessaria |
|---|---|
| USB sola | Il mux deve alimentare MCU/recovery lasciando il ramo OBD inattivo. Nessuna alimentazione CAN o consenso TX dedotto dalla sola USB. |
| OBD attiva, USB assente | Il mux deve impedire che il5V OBD alimenti il preregolatore e la porta. Verificare anche leakage e pull-up dei segnali di stato. |
| Entrambe presenti | Priorità OBD del mux e transizione senza dropout/battimenti fra buck; misurare corrente sulla porta inattiva e B+. |
| OBD standby, USB presente | Il service può essere alimentato dalla USB, ma non deve riattivare CAN né nascondere consumo OBD anomalo. |
| USB rimossa o sorgente a0V con cavo inserito | Controllare reverse transient di entrambi i buck e del mux; nessun semplice EN basso è assunto come isolamento bidirezionale. |

TPS25940 ha soglia reverse−77…−55mV e risposta tipica dipendente dall’overdrive; leakage di uscita nello stato off è specificato in condizioni con ingresso2,7V, non automaticamente ingresso0V. TPS259631 non offre una specifica di reverse blocking: il sistema deve affidarsi a un elemento qualificato e misurare il percorso completo. Il TPS2116 blocca corrente inversa secondo le proprie condizioni, con ritardo/leakage non nulli. Nel prototipo non sono stati collegati FLT/PG a pull-up alimentati da altri domini.

La capacità fisica di3,3V disponibile al MCU, la corrente consentita dalla porta e la disponibilità di alimentazione durante ROM sono tre requisiti distinti. Nessuna delle tre architetture elimina CC/enumerazione o il consumo da misurare prima che il firmware applicativo possa ridurre il carico.

## HR-01: tre alternative al fault del bypass SLEEP

### A — LM74910H-Q1 al posto di LM74900-Q1

La tabella di famiglia del [datasheet LM749x0 Rev.C](https://www.ti.com/lit/ds/symlink/lm74900-q1.pdf) distingue chiaramente LM74910H: retry in sovracorrente SLEEP, anziché latch-off. Durante il retry passa al percorso MOSFET normale per64cicli del timer. A12V il massimo IQ normale è810µA, rispetto a10µA massimi SLEEP. Il fault termico SLEEP richiede ancora un toggle SLEEP o EN; pertanto non sostituisce un’architettura di recupero da tutti i guasti.

Screening statico dei tre elementi originali:10+30+15=55µA. La condizione del massimo shutdown15µA del LM5164 è a24V: non è una misura/garanzia del sistema12V. In retry il solo controller può eccedere100µA. Una media `55 + duty×800µA` lascerebbe al massimo duty5,625% prima di ogni altro consumo, **ma non soddisfa un requisito massimo istantaneo**. Non si cambia la definizione del target in una media.

È una sostituzione candidata con interesse per la continuità AON, non un drop-in approvato. Corrente shunt, timer, limiti, MOSFET e sequenza vanno riesaminati. [TI part-details LM74910HQRGERQ1](https://www.ti.com/product/LM74910H-Q1/part-details/LM74910HQRGERQ1) non mostrava stock acquistabile nella sessione anonima: nessuna disponibilità garantita.

### B — Protezione a basso IQ sempre attiva

Il [LM74721-Q1 Rev.B](https://www.ti.com/lit/ds/symlink/lm74721-q1.pdf) mantiene il percorso di alimentazione protetto senza ricorrere al bypass SLEEP. Il TCAN resta sul nodo protetto a monte del buck commutato; INH controlla il buck tramite livelli/temporizzazioni adatti. L’evento di wake non dipende dal ripristino di un bypass che alimenta lo stesso transceiver.

Il massimo IQ pubblicato è35µA a12V, EN alto, controller in regolazione, fino a125°C; la pagina commerciale riporta anche un dato parametrico38µA non identico. Lo screening da datasheet dà35+30+15=80µA e solo20µA residui; usando prudenzialmente38 diventano83µA, residuo17µA. Questo controller offre reverse/OVP e controllo FET ma **non replica la funzione OCP del LM74900**. L’alternativa richiede una protezione di corrente esplicita e coordinata con fusibile/cavo/SOA; non è autorizzata la sua eliminazione.

Il ramo always-on non bypassa la protezione: utilizza l’uscita della nuova protezione completa, con inversione, OV e limitazione/fusibile qualificati. Il claim “TVS-less” del componente non giustifica eliminare i transitori scelti o trasferire la prova del produttore all’assieme. Questa alternativa è promettente per togliere il deadlock, ma più vicina al limite standby e con nuova ingegneria di fault.

### C — Controller esistente con reset indipendente

Un circuito di rilevamento/pulse può azionare EN o SLEEP anche quando AON è collassato, ma deve rimanere alimentato **da un ramo autonomo provvisto delle proprie protezioni contro inversione e OV**. Non collegare un timer direttamente al B+ grezzo, né chiamare “protetto” il nodo intermedio senza verificarne il clamp. Il nodo intermedio del controller principale non dispone della stessa OV cutoff dell’uscita.

Elementi di studio: [TPS7A16-Q1](https://www.ti.com/lit/ds/symlink/tps7a16-q1.pdf) per un rail ausiliario3,3V e [TPL5111](https://www.ti.com/lit/ds/symlink/tpl5111.pdf) come timer. Il primo dà15µA massimi aIOUT10µA, oltre al carico fornito; il secondo50nA massimi a2,5V/25°C nella tabella, con conversione del resistore fino a400µA per120ms. Non estendere quei50nA a3,3V e a ogni temperatura come worst-case garantito.

Subtotal puramente orientativo55+15+0,05=70,05µA, residuo29,95µA: mancano protezione del ramo, partitore LDO/regolatore fisso, rilevatore fault, level-shift HV, corrente dei pull-up e gate, leakage e transitori. Il timer da solo non costituisce un reset corretto: servono qualificazione del fault, impulso garantito e protezione dai cicli di riavvio. Un reset periodico indiscriminato può riattivare il carico a quadroOFF e fallire il target. EN attualmente è collegato direttamente aVS: prima di aggiungere un open-drain occorre ridisegnarne il collegamento, non cortocircuitareVS a massa.

## Decisioni e verifiche prima di integrare

Non serve chiedere ora una nuova decisione prodotto per continuare simulazione, sourcing e dimensionamento. Nessun target automotive, latenza di wake o temperatura è congelato da questa analisi. Prima di rendere la USB operativa serviranno un budget misurato della recovery ROM e una strategia qualificata CC/corrente; se la recovery risultasse impossibile con porte a corrente minima, l’eventuale vincolo su sorgente/cavo andrebbe presentato al proprietario con i numeri, senza introdurlo tacitamente.

Per il prossimo incremento HW-02: studiare preregolatore4,2V H, protezione con soglia più alta ma compatibile con il suo ingresso, passivi con curve bias/DCR, simulazione hot-plug e misura delle due conversioni; verificare soglia/PG e sequenza MCU. Per HR-01: mettere a confronto A eB con diagramma di stati, OCP/fuse/SOA e tabella corrente completa. C richiede più circuiteria e rimane riserva, non una scorciatoia al budget.

Prove richieste: hot-plug ai due ordini; fault USB con fronti ed energia definiti; VBUS5,5V; USB minima a ogni carico di boot; dropout e brownout; corrente inversa/porta a0V; fault OCP/TSD SLEEP e recupero senza unplug OBD; consumo e wake spurii a quadroOFF. Nessuna prova effettuata.

## Riproduzione

`python3 scripts/analyze_usb_power.py` genera6casi OVP,36sensibilità carica fault,4scenari inrush,3screening current-limit,2finestre preregolatore,240punti potenza cascata e4subtotal wake. L’autocontrollo iniziale verifica la formula del partitore anche col leakage. CSV eJSON non contengono alcun PASS hardware.

Gli output restano in questa directory. `source_manifest.json` registra gli hash dei PDF scaricati; le copie originali già in `evidence/` non sono state modificate. Nessun fileA0 o script di altri filoni è stato riscritto.
