# Buck 5 V — screening del feedback LM5164-Q1

2026-10-24. Generato da `scripts/build_buck5_screen.py`. Studio di due reti alternative; nessuna scelta definitiva di componenti o dichiarazione di stabilità.

## Risultato

La rete studiata per circa20mV a12V non mantiene12mV quando la tensione al buck scende verso6V. La seconda aumenta il ripple a bassa tensione, ma introduce un errore DC maggiore. Nessuna delle due è qualificata: R210/C208/C209 restanoTBD nello schema.

| Rete di studio | VIN al buck V | Ripple FB nominale mV | Minimo nei corner ideali mV | Picco induttore a1A, corner A |
|---|---:|---:|---:|---:|
| nominal_12V | 6 | 6.29 | 4.98 | 1.043 |
| nominal_12V | 8 | 14.08 | 12.48 | 1.087 |
| nominal_12V | 12 | 21.87 | 19.97 | 1.132 |
| nominal_12V | 18 | 27.07 | 24.97 | 1.162 |
| nominal_12V | 40 | 32.78 | 30.47 | 1.195 |
| low_input_study | 6 | 15.38 | 12.18 | 1.043 |
| low_input_study | 8 | 34.42 | 30.51 | 1.087 |
| low_input_study | 12 | 53.47 | 48.83 | 1.132 |
| low_input_study | 18 | 66.16 | 61.04 | 1.162 |
| low_input_study | 40 | 80.13 | 74.48 | 1.195 |

## Metodo e ipotesi

FonteE04: datasheetTI LM5164-Q1 Rev.C, equazioni15/18/21/24/25/26. RON41,2kΩ, partitore316kΩ/100kΩ, L47µH: valori inizialiA0. VOUT ideale4,992V e FSW≈303kHz. VIN è misurata sul pin2 del buck dopo cavo/protezioni:6V qui non sono6V alla presaOBD.

Rete nominal_12V: RA200kΩ, CA2,2nF. Rete low_input_study: RA100kΩ, CA1,8nF. CB100pF solo per studio, con ipotesi75µs per la formula26; non garantisce quel tempo di risposta. Nessun MPN selezionato e nessuna disponibilità verificata per questi passivi.

FSW=VOUT×2,5·10⁹/RON; tON=VOUT/(VIN×FSW); ΔVFB≈tON×(VIN−VOUT)/(RA×CA); ΔIL=VOUT×(1−VOUT/VIN)/(FSW×L). COUTeff=22µF è un’ipotesi per il condensatore nominale47µF, non una curva DC-bias verificata. Ripple capacitivo=ΔIL/(8×FSW×COUTeff); non includeESR, ringing o transitori di carico.

64corner per punto: VREF1,181/1,218V (limiti datasheet), quattro resistenze ±1% e CA±5% (ipotesi). Per il picco si usa L ridotta del20% (ipotesi). Questi estremi non includono dispersione del timing interno, temperatura dei passivi, ESR/DCR, perditeFET o bias dei condensatori. La tabella timingTI fornisce valori tipici: non trasformare il calcolo in una garanzia worst-case.

Il solo partitore, includendo i limitiVREF e resistenze±1%, dà 4.839…5.145V prima dell’errore del ripple. CAmin nominale=435pF; CBmin per75µs=79.1pF.

## Vincoli prima di scegliere la rete

TI raccomanda circa20mV al punto nominale e almeno12mV all’ingresso minimo. Per la rete low_input_study il ripple nominale a12V è circa54mV: la stima di errore DC è metà ripple moltiplicata per4,16, cioè circa112mV in uscita. Non sommare questa stima ai limiti statici per dichiarare una finestra garantita. Occorre risolvere il punto di lavoro reale e verificare compatibilitàTCAN/mux/rail3,3V.

E52 mostra che ampiezza del ripple da sola non dimostra stabilità: il ripple capacitivo d’uscita sfasato può dominare il feedback. Servono analisi completa della rete, simulazione e prove di step carico/linea, DEM e avvio. Anche il superamento dello screening12mV resta solo un controllo numerico preliminare.

Il picco va confrontato con il minimo limiteHS1,25A; Isat deve invece coprire il massimo limite1,75A più overshoot/delay e derating, come richiesto dal datasheet. Il modello non seleziona l’induttore. A40V il calcolo esamina sensibilità del nodo protetto: non autorizza uso nominale24V e non definisce il clamp automotive.

La continuità a6V non è dimostrata: dropout, cadute e limitazione di corrente possono invalidare la leggeCCM ideale. DEC-008 consente reset al crank; soglieUV/OV e recupero automatico richiedono progetto separato.

## Riproduzione e chiusura

`python3 scripts/build_buck5_screen.py` rigenera questo documento, CSV eJSON. Una verifica numerica riproduce l’esempio pubblicatoTI48V→12V (300kHz, ~20mV, ~0,44A ripple) per intercettare errori di unità nelle equazioni.

Prima di assegnare valori nello schema: fissare la finestra del nodo protetto insieme al wake/UVLO; verificare il budget5V reale; scegliereMPN e curve dei passivi; eseguire analisi di stabilità e bench. Nessun valoreTBD è stato sostituito con una falsa scelta finale.

Fonti: [E04 datasheetTI](https://www.ti.com/lit/ds/symlink/lm5164-q1.pdf), [E52 notaTI SNVA874](https://www.ti.com/lit/an/snva874/snva874.pdf).
