# Verifica visiva APP-09

Screenshot del processo iOS realmente compilato, installato e avviato su iPhone17Pro simulato, runtimeiOS26.4, Xcode27.0, targetiOS17. SwiftUI Pro applicata a componenti, stato, accessibilità e dimensionamento. Sono immagini della UI nativa completa, con navigazione/tabbar, distinte dai rendering APP-04.

- `simulator-history.png`: ingresso e filtri compatti; titolo «Misure nel tempo», pulsante back e banner di simulazione visibili. Il grafico compare nel primo schermo.
- `simulator-chart.png`: grafico scrollato, quattro campioni realmente presenti nella copia locale, senza waveform inventata. La linea costante riflette il raw1a10,1668giri/min.
- `simulator-table.png`: tabella scrollata con valori, qualità, raw e contatori monotoni originali.
- `simulator-chart-dark.png`: schema scuro.
- `simulator-table-accessibility.png`: dimensione testo `accessibility-extra-large`, contenuto scorrevole e testo su più righe.

Origine: archivio locale effettivamente importato dalla fixture gaugeSIMULATO-001, sessione0505…, non dati di un veicolo reale. Gli argomenti Debug `--history-timeline`, `--history-timeline-chart` e `--history-timeline-table` cambiano soltanto navigazione/posizione di scroll; non generano campioni e non attivano radio o rete. Dopo le acquisizioni sono ripristinati testoLarge e aspettoLight del simulatore.

Ispezione statica di wrapping, contrasto visivo, linee, righe e barre. Il back nativo è ora visibile dopo aver spostato il banner dentro i contenuti NavigationStack. Non è stato possibile pilotare Simulator con CUA in questa sessione: tap/gesture di ritorno e collaudo interattivo VoiceOver non sono stati eseguiti. Non è una certificazione completa di accessibilità né una prova su dispositivo fisico. I controlli usano API native, targetminimi44pt e label italiane con contatori esatti.
