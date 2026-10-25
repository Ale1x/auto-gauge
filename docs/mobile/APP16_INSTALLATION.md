# APP-16 — preparazione e trasferimento OTA raggiungibili dalla UI

25 ottobre 2026. iOS17, SwiftUI nativo, nessun framework aggiunto. Il percorso configurato usa i codec/verifier/sender già disponibili, senza downloader o cloud live.

## Collegamento applicativo

`AppModel` accetta `firmwareTrust: OTAPreparationTrust?` e `otaInstallationProvider: (any OTAInstallationSessionProvider)?`. Conserva un unico `OTAInstallationController` per la propria durata. `FirmwareView` prende queste dipendenze dal modello e le passa a `FirmwarePreparationModel`/view; riaprire la schermata non crea un nuovo sender dopo UNKNOWN.

Il normale avvio non ha profilo fidato né provider qualificato. La UI consente di scegliere i file ma spiega le dipendenze assenti. Quando il codice di integrazione locale fornisce entrambe, la stessa UI chiama il core: verifica offline → copia immutabile verificata → `openSession` → upload con progresso ACK → READY → azione separata e confermata di select. Non viene creato un fake provider per abilitare i pulsanti.

`OTAInstallationSessionProvider.openSession(for:deadline:)` è il confine fiduciario locale: il provider deve verificare identità/hardware/policy corrente e ottenere la route OTA esclusiva con una lease fresca; la sessione ritornata deve ricontrollare autorizzazione/generazione per ogni operazione e implementare `close`. Il provider non può derivare la decisione da hello. Deve anche riconciliare un tentativo precedente interrotto prima di concedere una nuova sessione dopo riavvio dell'app. Il controller generico non implementa né certifica la qualifica Bluetooth/target: serve la futura integrazione piattaforma qualificata.

Per sessioni AG su pipe il backend esistente `OTAPacketTransport` implementa `OTAInstallationSession`, usa eventuali finestre `GaugeRPCExchangePipe.beginRequest/endRequest` e può aprire con `open(expectedConnection:)`. Il provider qualificato deve passare la connessione osservata per evitare una corsa tra qualificaA e connessioneB. Un mismatch non disconnetteB: cleanup indirizza soltantoA.

## Stati e comportamento

- Nessun provider: nessuna apertura/scrittura. Nessuna qualifica implicita dall'importazione file.
- Opening/uploading: provider e sender hanno deadline e cancellazione cooperativa. La UI disabilita modifiche ai file durante l'operazione e dopo il primo tentativo.
- Il progresso contiene soltanto offset accettati in ACK validati dal sender; non stima byte installati o boot riuscito.
- Upload termina a READY. Nessun select automatico; il pulsante separato apre un dialogo di conferma.
- SELECTED significa selezione confermata, non firmware avviato/sano. Il controller chiude la sessione dopo select.
- Errore o annullamento dopo apertura: UNKNOWN terminale e chiusura della lease; nessun retry/reset/resume o cancel wire automatico. Non si dichiara rollback dei byte già scritti.
- Annullare o lasciare la schermata durante READY chiude la lease e richiede riconciliazione; non seleziona il nuovo firmware.
- Il manifesto dell'operazione appartiene allo snapshot del controller e non viene sostituito dalla riapertura di un'altra preparazione locale.

UNKNOWN è conservato nel controller per tutta la durata di AppModel, anche navigando fuori e dentro. Non è un journal di installazione persistente nel telefono: dopo un crash l'integrazione provider deve riconciliare lo stato target prima di aprire un'altra route. L'assenza del provider predefinito impedisce oggi di avviare questo percorso su radio non qualificata.

## Verifiche

APP16InstallationControllerTests usa candidati verificati con le firme RSA reali dei vettori comuni e provider/sessioni locali espliciti di test. Copre assenza provider, progressi dagli ACK, finish/select separati, richiesta ripetuta rifiutata, ACK upload/select perduti, generazione cambiata, cancellazione prima/dopo consegna, lease tardiva chiusa, READY abbandonato, provider rifiutato, busy e timeout apertura.

I test OTAPacketTransport sono estesi alle finestre native esclusive e alla corsa expected-lease A/B. Queste prove non qualificano radio, flash, NVS, boot o memoria del prodotto e non aprono il gate predefinito.
