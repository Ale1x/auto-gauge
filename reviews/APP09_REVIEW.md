# Review APP-09 del coordinatore

24 ottobre 2026. Lettura separata dall’autore di `GaugeCore/Timeline`, relative view/model SwiftUI, route e12test `HistoryTimelineTests`. Applicata SwiftUI Pro, con riferimenti data flow e accessibilità; target iOS17 conservato, nessun framework esterno.

Nessun difetto concreto aperto nel perimetro letto. Il coordinatore ha verificato:

- Le sette formule coincidono con il sottoinsieme qualificato del decoder C. Raw di lunghezza errata, formule assenti e stati non validi non producono valori numerici; uno zero valido resta uno zero.
- Sessione, PID ed ECU non vengono mescolati. Le perdite osservate possono convivere con record recuperati da altri telefoni; un intervallo di sequenze non viene convertito in una durata inventata. Anche una sessione con sole perdite resta visibile senza PID fittizio.
- La sottrazione dell’origine usa UInt64 prima di convertire in Double. Record e tabella conservano i contatori esatti; la precisione grafica limitata viene segnalata. Regressioni temporali non sono nascoste riordinando per timestamp.
- Riduzione bounded a240punti originali, estremi/endpoint conservati e segmenti interrotti su gap/stati invalidi. Se il budget non conserva tutte le interruzioni, il grafico mostra solo punti. La tabella mantiene tutti i record selezionati,50per pagina.
- Modello UI MainActor/Observable; lavoro separato e cancellabile con token di generazione prima di pubblicare il risultato. Nessun nuovo accesso radio, database remoto o mutazione agli archivi.

Due rilievi visivi sono stati corretti durante questa review. L’introduzione lunga e il SID completo spingevano il grafico sotto il primo schermo: ora provenienza compatta e filtri precedono il grafico, mentre «Origine e tempi» è espandibile e il selettore dedicato conserva il SID intero. Il banner sul TabView copriva titolo/back nella destinazione inline: l’inset è stato spostato nei contenuti dei NavigationStack, conservando il banner sulle pagine. Il coordinatore ha ispezionato lo screenshot finale con titolo «Misure nel tempo» e back nativo chiaramente visibili.

Evidenze:117testSwift totali superati nella verifica integrata, inclusi12nuovi casi timeline; screenshot del simulatore per primo schermo/grafico/tabella, tema scuro e testo di accessibilità. I raw1a10 del file demo producono1668giri/min e contatori2000/3000ms coerenti. I PNG sono in `mobile/DesignValidation/APP09/`. Sono render reali, non mockup; i dati restano simulati.

Limiti: screenshot e sorgente non dimostrano VoiceOver, tap/gesti su iPhone reale, sospensione/background o integrazione veicolo. Il catalogo grafico app rimane7formule, anche se il decoder firmware ne contiene53. La vista usa archivi locali bounded, non introduce una politica di retention V1 o streaming live. Build e verifica visuale finali dell’autore sono documentate nel README mobile.
