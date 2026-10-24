# Review APP-10 del coordinatore

24 ottobre 2026. Lettura separata dall’autore di `HistoryPIDDefinition`, `CatalogView`, aggiornamenti timeline e `HistoryScalarCatalogTests`. Target iOS17, struttura SwiftUI Pro e percorsi BLE/cloud invariati.

Nessun difetto concreto rilevato nel perimetro. I53PID scalari coincidono con l’allowlist firmware; formule unsigned big-endian1/2/4byte, unità e lunghezze sono confrontate con212vettori prodotti eseguendo il vero decoder C sanitizzato. L’oracle non analizza la tabella C per riscriverla in Swift: chiama `ag_obd_parse_measurement` e verifica che il fixture salvato sia ancora aggiornato in ogni check host.

Conservati frazioni, segno degli offset e U32 prima della conversione: rapporto44 con raw8000=1, carico assoluto43 fino25700%, odometroA6 fino429496729,5km,5D senza troncamento. Non vengono applicati clamp estetici. Payload di lunghezza sbagliata o non canonici non sono decodificati per prefisso; stati non validi non diventano valori. I raw rimangono immutati.

Il catalogo distingue le formule disponibili per gli storici dalle capability dichiarate dal simulatore. L’elenco a53 non abilita nuovi widget, non modifica discovery o supporto ECU e non apre gate BLE. Le etichette distinguono tensione centralina, rapporto comandato e vita residua pacco ibrido.

Evidenze:121testSwift complessivi verdi, inclusi4nuovi test che coprono oracle/min/mid/max/pattern, unità, payload e6stati per ciascuno dei53PID; build iOSsim e genericdevice unsigned riuscite. Non eseguite discovery, radio, sessioni veicolo o prove dei singoli PID su un’auto. Le formule corrispondenti fra due implementazioni non costituiscono una nuova certificazione dello standard OBD o della disponibilità dei dati.
