# Studio meccanico K0 — preliminare, fit respinto

Vincolo: DEC-001, Ø esterno massimo 44,45 mm. Per dimensioni del candidato e disegno originale vedere E01 in evidence/SOURCES.md e la tavola evidence/panel-mechanical.png.

| Quantità | Risultato | Tipo di dato |
|---|---:|---|
| 1,75 × 25,4 | 44,45 mm | conversione esatta |
| Cover touch candidata | 48,96 mm | nominale datasheet, non misura di campione |
| Superamento già senza case | 4,51 mm sul diametro | calcolo |
| Ingombro massimo pannello senza cover | 46,35 mm | nominale datasheet |
| Superamento senza cover | 1,90 mm | calcolo; togliere la cover non risolve |
| Spazio radiale fra area attiva e limite esterno | (44,45 − 43,76)/2 = 0,345 mm | calcolo; non include bordo pannello/touch/case |

Due disposizioni concettuali, stesso pannello e un solo PCB:

| Elemento | A: cavo posteriore assiale | B: cavo laterale/radiale |
|---|---|---|
| Display + touch | Stesso candidato fuori sagoma | Stesso candidato fuori sagoma |
| PCB | Dietro pannello; Ø < 44,45 mm meno pareti/gioco, valore non congelabile | Idem; ulteriore area richiesta dalla presa laterale |
| Connettore gauge | Presa posteriore, spazio condiviso con magnete | Presa nel fianco; sporgenza e plug accoppiato da includere nel drawing |
| USB-C nascosta | Accesso posteriore dietro copertura rimovibile | Accesso laterale sotto tappo; contesa con presa cavo |
| Magnete | Posteriore, possibile interferenza con presa | Posteriore, possibile maggiore area libera |
| Sensore luce | Apertura frontale/periferica da verificare | Idem |
| BLE | Antenna al margine, lontano da magnete e metallo; keep-out da datasheet | Idem; presa e cavo possono ridurre lo spazio RF |
| Sezione/spessore | FPC, altezza componenti, piega e assemblaggio non definiti | Idem |
| Esito a Ø44,45 mm | **Non fattibile con E01** | **Non fattibile con E01** |

Non sono due alternative approvate né un fit CAD. Non sono stati inventati quote di connettori, magnete o case. La quota dichiarata di 48,96 mm è sufficiente a respingere entrambe, indipendentemente dallo spessore. La rotazione di un oggetto non riduce il suo diametro d'inviluppo.

Il task dei due disegni quotati resta BLOCCATO, non completato. Dopo un pannello conforme: acquisire DXF/STEP, tolleranze, FPC e raggio minimo, fissare inviluppi delle prese e del plug accoppiato, sezioni dell'assieme, RF keep-out, strain relief e metodo di montaggio magnetico; confrontare A/B in CAD prima di K1.
