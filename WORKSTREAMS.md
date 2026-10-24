# Lavoro parallelo — 24 ottobre 2026

Il proprietario ha autorizzato di dividere il lavoro tra più agenti e di iniziare app mobile e firmware. Questo avvio non approva il gate hardware, ordini, distribuzione dell'app o programmazione del dispositivo.

| Filone | Responsabilità esclusiva | Primo risultato verificabile |
|---|---|---|
| App iOS | `mobile/` | App SwiftUI italiana, modalità simulata, editor/configurazione, test modelli e build simulatore |
| Firmware | `firmware/` | Nucleo C testabile su host, guardie CAN, validazione config, log bounded, skeleton ESP-IDF |
| Review hardware | `hardware/reviews/A0_INDEPENDENT_REVIEW.md` | Revisione separata di pin/net/power/wake rispetto a schema e fonti |
| Coordinamento | `shared/`, integrazione e stato progetto | Contratto comune, fixture, confronto delle implementazioni e verifiche finali |

Gli agenti lavorano contemporaneamente in directory distinte nello stesso repository; i commit sono gestiti dal coordinatore. Nessun agente modifica file di un altro filone. Le scelte tecniche provvisorie sono marcate come ipotesi, senza inventare decisioni di prodotto.

## Dipendenze

App e firmware condividono `shared/PROTOCOL_V0.1.md` e la fixture di configurazione. Possono progredire con simulatori e HAL finché pinout pannello, wake/IGN e protezioni non sono chiusi. La revisione hardware alimenta la prossima iterazione dello schema, non una dichiarazione di costruibilità.

Backend account/cloud, trasferimento BLE autenticato, policy ownership e OTA sicura restano da progettare. Il primo incremento non deve simulare questi servizi come se fossero operativi. Prima della connessione reale: fissare trasporto/sicurezza, testare interoperabilità, compilare SDK device e provare su banco.

## Primo incremento completato

| Filone | Risultato | Verifica eseguita |
|---|---|---|
| `app_ios` | App SwiftUI con editor, configurazione, luminosità e storici simulati; account/OTA reali non disponibili | 11 test Swift, tutti i 22 casi condivisi, build iOS simulatore. Installazione e avvio su iPhone 17 Pro/iOS 26.4 confermati dal processo; UI non ispezionata visivamente |
| `firmware` | Nucleo C, HAL disabilitata, skeleton ESP-IDF e test host | Build Clang con ASan/UBSan e CTest superati, 19 dei 22 casi condivisi in strutture C. I tre casi di tipo JSON attendono il parser |
| `review_hardware` | Report HR-01…HR-08, 53 collegamenti critici confrontati; successiva lettura del core firmware | Nessuna nuova inversione di pin; nessun bypass CAN/accesso fuori array trovato nei percorsi software esaminati. Review limitata, nessun gate approvato |
| Coordinamento | Contratto e fixture comuni, correzioni di integrazione e comando unico | `python3 scripts/check_software.py` completato con successo il 24/10/2026 |

Il coordinatore ha richiesto e verificato due correzioni nell'app: le capability reali devono essere l'autorità sui PID, senza dipendere dal catalogo demo; la quantizzazione della geometria non deve produrre rettangoli invalidi. Il catalogo mock e l'allowlist firmware ora concordano sul PID 0F. Le revisioni sono salvate nello stesso repository, senza pubblicazione remota.

Dettagli e limiti delle prove in `shared/VERIFICATION_V0.1.md`. Il primo incremento dei tre agenti è concluso; questo file è il punto di ripartenza per le assegnazioni successive.

## Prossimo gruppo di attività

| ID | Responsabile | Lavoro | Dipendenza |
|---|---|---|---|
| APP-02 | App iOS | Migliorare editor e gestione locale delle configurazioni; testare navigazione e resa circolare | O-10 da tradurre in proposta, senza congelare regole non approvate |
| FW-02 | Firmware | Decoder Service 01, discovery e ISO-TP su simulatore; scheduler con timeout e dati indisponibili | Documentazione protocolli e fixture ECU; nessuna necessità di trasmettere su vettura |
| SH-02 | Coordinamento con app e firmware | Definire framing e trasferimento bounded, sessione autenticata, ownership e conferme distinte | Policy ownership/recupero e sicurezza da progettare e approvare prima dell'uso reale |
| HW-02 | Hardware | Recupero fault/wake e funzione serie USB, poi alimentazioni/passivi | HR-01…HR-04, limiti elettrici e prove di banco |
| INT-02 | Integrazione | Collegare app a un simulatore del protocollo device, import log con deduplica/resume | Contratto di messaggi e parser bounded; niente dipendenza da cloud per il gauge |

APP-02, FW-02 e HW-02 possono avanzare in parallelo. INT-02 segue la prima definizione di SH-02. Il backend cloud può restare un'interfaccia finché provider e policy non sono scelti; nessun falso login o servizio esterno è necessario per continuare.
