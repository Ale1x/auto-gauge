# HW-IGN — protocollo operativo K0, NON ESEGUITO

Data 2026-10-24. Requisiti: display ON con quadro ON anche a motore OFF; nessun polling di wake in standby; DEC-005 ≤100µA a12V entro300s dal quadro OFF. Prove su simulatore e banco prima della Polo effettiva. Nessuna compatibilità vettura dichiarata.

## Prerequisiti e strumenti

Alimentatore limitato in corrente, protezione cablaggio e circuito di test verificati; logger CAN realmente listen-only senza terminazione aggiuntiva né ACK attivi; misura corrente con capacità di catturare sia µA sia picchi di boot, oppure due range con commutazione documentata; misura tensione ai capi del DUT. Non usare un multimetro con burden voltage ignoto per dichiarare stabilità. USB e strumenti con massa comune possono creare percorsi che falsano corrente o rete: documentare isolamento/topologia e misurare il solo B+ del DUT.

Registrare strumenti, range, banda, frequenza campionamento, calibrazione, incertezza, temperatura, revisione hardware/firmware e pinout fixture. La risoluzione minima, il trattamento dei picchi e l'intervallo di osservazione per il limite100µA vanno fissati prima di dichiarare PASS; la media sola non prova il massimo. Nessun dato sintetico ammesso nel report di misura.

## Sequenza banco/simulatore

1. Ispezione/corti e rail con sorgente limitata; confermare che TX diagnostico sia bloccato in riposo, durante boot ambiguo e in fault. Nessun collegamento auto in questa fase.
2. Riprodurre CAN assente, attività continua e burst di wake; verificare la catena wake/INH/power, ritorno a riposo e assenza di trasmissioni risveglianti. Le annotazioni IGN del simulatore sono ground truth di prova, non un nuovo ingresso del prodotto.
3. Provare reset/bus-off/brownout e logger pieno/flash in scrittura; verificare che un guasto non lasci il CAN dominante o il dispositivo permanentemente attivo.
4. Registrare curva corrente, non solo il valore finale. Verificare ingresso nello stato ≤100µA entro300s dall'evento OFF e persistenza successiva; annotare ogni wake. Durata di osservazione proposta2ore e poi overnight: ipotesi di procedura, non criterio già approvato.
5. Solo dopo esito banco documentato, passare all'osservazione sulla vettura. Non fare test transienti/load dump sull'auto.

## Matrice vettura, prima osservazione passiva

| Scenario | Ground truth e dato da acquisire |
|---|---|
| Quadro ON, motore OFF | Timestamp manuale sincronizzato; traffico CAN osservabile e stabilità display quando prototipo ammesso |
| Motore acceso e avviamento | Tensione, transizioni di comunicazione; non dedurre IGN dalla sola tensione |
| Quadro OFF, porte chiuse | Tempo fino a quiete, attività residua, corrente DUT, nessuna richiesta diagnostica |
| OFF, apertura/chiusura porte e lock/unlock | Falsi wake e loro durata/energia; verificare che non spostino indefinitamente il termine300s |
| OFF con caricabatteria | Tensione elevata senza IGN: verificare falsi positivi |
| Riaccensione dopo lungo riposo | Evento CAN realmente visibile, wake e dashboard; latenza massima ancora O-06 |

Prima caratterizzazione con solo logger passivo approvato e sorgente di ground truth esterna: non serve ordinare già il gauge finale. Identificare vettura effettiva e adattatore senza trasmettere o codificare. Annotare ogni strumento che possa tenere sveglio il gateway.

Se il gateway non espone un segnale distinguibile ON/OFF con metodi standard e osservazione passiva, registrare il fallimento del candidato HW-IGN. Non introdurre una regola OEM o una richiesta periodica per aggirarlo; occorre una decisione sul compromesso prodotto. La possibilità di leggere PID in stato attivo non dimostra il wake da veicolo inattivo.

## File di acquisizione

Usare tests/templates/hw_ign_samples.csv con timestamp monotono comune, IGN annotato e corrente/tensione DUT. I contatori DUT di TX/requests sono cumulativi e vanno correlati con cattura esterna; il semplice traffico totale sul bus non attribuisce i frame al gauge. Segnare UNKNOWN dove manca ground truth, non convertire in0. Conservare raw trace e metadati; un file vuoto non è una prova passata.

Esito attuale: NON ESEGUITO. Mancano hardware/fixture verificati, strumenti e accesso alla Polo. Piano pronto per acquisizione, nessuna simulazione presentata come misura reale.

Strumento preparatorio disponibile: scripts/analyze_hw_ign.py; uso e limiti in tests/templates/README.md. Evidenzia osservazioni da revisionare e dati non validi, senza emettere un PASS. Il template originale resta vuoto.

DEC-008 ammette reset durante crank con ripartenza automatica: aggiungere interruzioni/brownout durante scrittura log, cambio configurazione e aggiornamento, verificando recupero, dati validi e avvio con TX diagnostico inibito finché lo stato non è qualificato. Tempo massimo di recupero e profilo restano da fissare; nessuna prova eseguita.
