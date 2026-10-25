# Standby — target approvato e primo controllo quantitativo

Data 2026-10-24. DEC-005: **massimo 100 µA all'ingresso OBD a 12 V entro 5 minuti dal quadro OFF**. Target approvato, risultato non dimostrato. Condizioni termiche, banda di misura e trattamento dei picchi restano da specificare: non si sostituisce arbitrariamente il massimo con una media permissiva.

## Componenti candidati: dati documentali, non misure

| Elemento | Condizione documentale | Corrente | Fonte |
|---|---|---:|---|
| TCAN1043A-Q1 VSUP | Sleep, CAN autonomous inactive | 18 µA tip., 30 µA max | E03 §5.7, p.6 |
| LM74900-Q1 | Esercizio normale a 12 V | 630 µA tip., 750 µA max | E05 §6.5, p.7 |
| LM74900-Q1 | SLEEP | 5,5 µA tip., 10 µA max | E05 §6.5, p.7 |
| LM5164-Q1 | Shutdown, VIN 24 V nella tabella | 3 µA tip., 15 µA max | E04 §5.5, p.5 |

Il solo controller di protezione lasciato in esercizio normale supera il target: questa configurazione è respinta per lo standby. Le alimentazioni VCC/VIO del transceiver, partitori, TVS, MOSFET, pull-up e leakage non spariscono dalla somma. I dati LM5164 a 24 V sono condizioni di caratterizzazione del componente, non supporto nominale a 24 V del prodotto né misura a 12 V.

## Allocazione preliminare

Somma orientativa dei massimi tabellari: 30 + 10 + 15 = **55 µA**; residuo contabile fino a100: **45 µA**. Non è un worst-case garantito del sistema: condizioni diverse, schema e percorso di wake non chiusi, altri rami non inclusi. Riservare parte del residuo a margine; non spenderlo tutto in partitori permanenti.

Il candidato LM74900 in SLEEP alimenta un carico always-on attraverso un percorso interno; all'uscita dal riposo occorre riattivare il percorso principale prima del carico MCU/display. Verificare handshake INH/SLEEP, latch-off, brownout e stato dei controlli quando il MCU è spento, senza deadlock di wake. Un MCU semplicemente in deep sleep non risolve automaticamente il consumo del resto della scheda.

Da dimensionare: dominio wake protetto da transitori e inversione, misura batteria disinseribile, spegnimento display/ALS/rail, isolamento GPIO/FPC e USB, enable del buck con livelli compatibili. Nessuna connessione diretta dal pin INH ad alta tensione ai GPIO a 3,3 V.

## Wake spurii e quadro OFF

Una visita di 5 s a 200 mA ogni ora aggiunge circa278 µA medi, già oltre100. Anche se il riposo statico passa, i falsi wake possono far fallire il requisito. Il tempo di 5 minuti approvato non permette di rinviare lo standby indefinitamente perché il CAN continua a parlare. Resta aperto O-18: se il traffico non distingue IGN, non si può dichiarare soddisfatto il requisito con un semplice timeout.

T-006/T-007 devono rilevare assorbimento completo e wake spurii insieme a ground truth del quadro. Misurare anche attività porte/chiusura/caricabatteria e log locali: nessun polling per riattivare ECU. Piano operativo in docs/hardware/HW_IGN_TEST_PROTOCOL.md. Nessuna prova eseguita.

Approfondimento: docs/hardware/POWER_ENVELOPE_K0.md e relativo CSV riproducibile. Comprende caduta cavo e perdite, vincolo del bypass SLEEP/avvio del carico e DEC-008 (reset crank ammesso con ripartenza automatica). Nessuna sezione/fusibile o protezione congelata.

Secondo stadio3,3V candidato: TPS62811QWRWYRQ1, dettagli e condizioni in docs/hardware/MCU_RAIL_K0.md. La corrente shutdown dipende dalla temperatura e non è automaticamente nulla. Mappare il dominio5V realmente spento e alimentazioni parassite prima di riportare la corrente all’ingresso12V; non sommare correnti misurate a tensioni diverse come se fossero sullo stesso nodo. Il budget55µA resta solo screening dei tre componenti originari.
