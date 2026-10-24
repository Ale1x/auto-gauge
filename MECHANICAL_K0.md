# Studio meccanico K0 — revisione 0.3, diametro da congelare

Vincolo corrente: DEC-002 autorizza un piccolo aumento del diametro esterno rispetto a 44,45 mm. Il display resta nominalmente 1,75″. **Ø52 mm è un'ipotesi di studio, non un massimo approvato né un fit CAD.**

Il precedente rifiuto del candidato E01 a Ø44,45 mm resta corretto storicamente ma non esclude più quel candidato dal nuovo studio. Fonti e disegno originale: evidence/SOURCES.md (E01), evidence/panel-mechanical.png. Dati nominali documentali, non misure di campione.

## Sensibilità radiale

Per la cover candidata nominale da 48,96 mm, il margine radiale lordo vale (D_case − 48,96)/2. È spazio per parete, tolleranze e gioco insieme, non tutto spazio libero.

| Diametro esterno ipotetico | Margine radiale lordo | Residuo con parete ipotetica 1,0 mm e gioco radiale 0,3 mm | Esito del solo calcolo radiale |
|---|---:|---:|---|
| 44,45 mm, precedente limite | -2,255 mm | -3,555 mm | Non entra |
| 50 mm | 0,52 mm | -0,78 mm | Insufficiente nelle ipotesi |
| 51 mm | 1,02 mm | -0,28 mm | Insufficiente nelle ipotesi |
| 52 mm | 1,52 mm | 0,22 mm | Candidato da approfondire |

Diametro minimo aritmetico nelle sole ipotesi sopra: 48,96 + 2×(1,0 + 0,3) = **51,56 mm**. Il gioco 0,3 mm non è una tolleranza garantita: deve ancora assorbire variazioni del pannello/case e montaggio. La parete 1,0 mm non è dimensionata per alcun materiale/processo. Ø52 aumenta il diametro di 7,55 mm (circa 17%) rispetto al precedente limite. Questo non dimostra la fattibilità dell'assieme.

## Due disposizioni da studiare con un solo PCB

| Elemento | A: cavo posteriore assiale | B: cavo laterale/radiale |
|---|---|---|
| Display + touch | E01 riammesso allo studio; fit completo non verificato | Idem |
| PCB | Dietro pannello; diametro da ricavare da appoggi, FPC e pareti | Idem, con area per presa laterale |
| Connettore gauge | Presa posteriore; contesa con magnete e supporto | Presa nel fianco; sporgenza del plug accoppiato da quotare |
| USB-C nascosta | Accesso posteriore sotto copertura di servizio | Accesso laterale sotto tappo |
| Magnete | Posteriore con spazio per presa; geometria TBD | Posteriore, area da confrontare con antenna |
| Sensore luce | Apertura periferica: visibilità sopra il bordo display da verificare | Idem |
| BLE | Antenna al margine, con keep-out verificato rispetto a pannello/metallo/magnete | Idem; presa laterale può sottrarre spazio RF |
| Sezione/spessore | Componenti, piega FPC, presa e supporto determinano profondità TBD | Componenti, FPC e prese laterali determinano profondità TBD |
| Esito a Ø52 ipotetico | Ammessa allo studio; nessun fit validato | Ammessa allo studio; nessun fit validato |

Il task dei due disegni quotati resta DA COMPLETARE. Serve prima documentazione corretta del pannello e delle parti reali: DXF/STEP, tolleranze, FPC e raggio minimo, inviluppi prese/plug, sezioni, keep-out RF e strain relief. Nessuna quota di presa, magnete, PCB o spessore è stata inventata. Le incongruenze elettriche/termiche e il sourcing E01 restano aperti.

## Tavola e nuove alternative

Tavola riproducibile: mechanical/gauge-k0-concepts.drawio.png (contiene XML editabile draw.io), generata da scripts/build_mechanical_concepts.py. Mostra fronte e disposizioni A/B; Ø52case e Ø48PCB sono H, modulo18×25,5mm è D. Prese, USB, magnete, FPC e sezione restano simbolici/TBD. Non è disegno produttivo né fit quotato completo.

Il calcolo radiale sopra riguarda solo E01. Il PDF VIEWE E20 cita48,16mm ma non è stato associato in modo univoco al prodotto venduto; non sostituire la quota nel modello senza conferma. LX E21 dichiara51,5mm: nelle stesse ipotesi di parete/gioco richiederebbe almeno54,1mm, quindi non entra in Ø52 con quelle ipotesi. Sourcing dettagliato in PANEL_SOURCING_K0.md.

RF: MEMORY_RF_K0.md registra la raccomandazione Espressif15mm e il conflitto con il piccolo anello radiale. La profondità totale resta TBD; non è stata verificata separazione dal pannello o dal magnete.
