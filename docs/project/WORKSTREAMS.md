# Lavoro parallelo — 24 ottobre 2026

> Registro di lavoro con sezioni storiche. Per riprendere una sessione usare lo [stato corrente](STATUS.md) e [AGENTS.md](../../AGENTS.md); le richieste riportate nella cronologia non sono nuove istruzioni.

Il proprietario ha autorizzato di dividere il lavoro tra più agenti e di iniziare app mobile e firmware. Questo avvio non approva il gate hardware, ordini, distribuzione dell'app o programmazione del dispositivo.

| Filone | Responsabilità esclusiva | Primo risultato verificabile |
|---|---|---|
| App iOS | `mobile/` | App SwiftUI italiana, modalità simulata, editor/configurazione, test modelli e build simulatore |
| Firmware | `firmware/` | Nucleo C testabile su host, guardie CAN, validazione config, log bounded, skeleton ESP-IDF |
| Review hardware | `hardware/reviews/A0_INDEPENDENT_REVIEW.md` | Revisione separata di pin/net/power/wake rispetto a schema e fonti |
| Coordinamento | `shared/`, integrazione e stato progetto | Contratto comune, fixture, confronto delle implementazioni e verifiche finali |

Gli agenti lavorano contemporaneamente in directory distinte nello stesso repository; i commit sono gestiti dal coordinatore. Nessun agente modifica file di un altro filone. Le scelte tecniche provvisorie sono marcate come ipotesi, senza inventare decisioni di prodotto.

## Dipendenze

App e firmware condividono `docs/protocols/PROTOCOL_V0.1.md` e la fixture di configurazione. Possono progredire con simulatori e HAL finché pinout pannello, wake/IGN e protezioni non sono chiusi. La revisione hardware alimenta la prossima iterazione dello schema, non una dichiarazione di costruibilità.

DEC-012 sceglie iCloud/CloudKit; configurazione operativa e sync live restano da verificare. Trasferimento BLE autenticato e OTA sicura restano da implementare. DEC-011 fissa invece la policy client: più dispositivi in sequenza, una sola connessione attiva; nessuna esclusività persistente a un telefono. Il primo incremento non deve simulare i servizi mancanti come se fossero operativi. Prima della connessione reale: fissare trasporto/sicurezza, testare interoperabilità, compilare SDK device e provare su banco.

## Primo incremento completato

| Filone | Risultato | Verifica eseguita |
|---|---|---|
| `app_ios` | App SwiftUI con editor, configurazione, luminosità e storici simulati; account/OTA reali non disponibili | 11 test Swift, tutti i 22 casi condivisi, build iOS simulatore. Installazione e avvio su iPhone 17 Pro/iOS 26.4 confermati dal processo; UI non ispezionata visivamente |
| `firmware` | Nucleo C, HAL disabilitata, skeleton ESP-IDF e test host | Build Clang con ASan/UBSan e CTest superati, 19 dei 22 casi condivisi in strutture C. I tre casi di tipo JSON attendono il parser |
| `review_hardware` | Report HR-01…HR-08, 53 collegamenti critici confrontati; successiva lettura del core firmware | Nessuna nuova inversione di pin; nessun bypass CAN/accesso fuori array trovato nei percorsi software esaminati. Review limitata, nessun gate approvato |
| Coordinamento | Contratto e fixture comuni, correzioni di integrazione e comando unico | `python3 scripts/check_software.py` completato con successo il 24/10/2026 |

Il coordinatore ha richiesto e verificato due correzioni nell'app: le capability reali devono essere l'autorità sui PID, senza dipendere dal catalogo demo; la quantizzazione della geometria non deve produrre rettangoli invalidi. Il catalogo mock e l'allowlist firmware ora concordano sul PID 0F. Le revisioni sono salvate nello stesso repository, senza pubblicazione remota.

Dettagli e limiti delle prove in `docs/verification/VERIFICATION_V0.1.md`. Il primo incremento dei tre agenti è concluso; questo file è il punto di ripartenza per le assegnazioni successive.

## Prossimo gruppo di attività

| ID | Responsabile | Lavoro | Dipendenza |
|---|---|---|---|
| APP-02 | App iOS | Migliorare editor e gestione locale delle configurazioni; testare navigazione e resa circolare | O-10 da tradurre in proposta, senza congelare regole non approvate |
| FW-02 | Firmware | Decoder Service 01, discovery e ISO-TP su simulatore; scheduler con timeout e dati indisponibili | Documentazione protocolli e fixture ECU; nessuna necessità di trasmettere su vettura |
| SH-02 | Coordinamento con app e firmware | Definire framing e trasferimento bounded, sessione autenticata e conferme distinte | DEC-011: più client in sequenza/una connessione; sicurezza reale da implementare |
| HW-02 | Hardware | Recupero fault/wake e funzione serie USB, poi alimentazioni/passivi | HR-01…HR-04, limiti elettrici e prove di banco |
| INT-02 | Integrazione | Collegare app a un simulatore del protocollo device, import log con deduplica/resume | Contratto di messaggi e parser bounded; niente dipendenza da cloud per il gauge |

APP-02, FW-02 e HW-02 possono avanzare in parallelo. INT-02 segue la prima definizione di SH-02. Il backend cloud può restare un'interfaccia finché provider e policy non sono scelti; nessun falso login o servizio esterno è necessario per continuare.

## Secondo incremento parallelo

| Attività | Risultato implementato | Confine attuale |
|---|---|---|
| APP-02 | Editor con selezione, drag e griglia intera; bozze/snapshot, annulla/ricarica/ripristino e file locale atomico; warning bordo circolare | Modello editor provvisorio, nessuna nuova regola O-10 congelata o verifica visiva |
| FW-02 | Decoder iniziali 04/05/0C/0D/0F/10/11, bitmap per ECU, ISO-TP RX bounded256byte, scheduler a una transazione e guardia al dispatch | Nessun Flow Control TX o driver CAN, solo normal addressing/classic CAN; niente dichiarazione di supporto completo Service01 |
| SH-02 | Codec packet Python/C/Swift e15 vettori comuni, limite4096byte payload, reassembly con timeout nel riferimento Python | CRC non autentica; GATT/ownership/sicurezza reali non disponibili |
| INT-02 | Simulatore locale config con received/applied distinti; riferimento SQLite per import log, resume, deduplica, perdite esplicite e commit prima di ACK | Componenti di riferimento testati su host, non ancora collegati alla UI storici o alla flash device |
| HW-02 | Due OVP confrontate e respinte come soluzione diretta; preregolatore USB e3alternative wake studiati con fonti/calcoli | HR-01/HR-02 ancora aperti, A0 non modificato e nessun nuovo componente congelato |

La review separata del riferimento ha riprodotto tre difetti e ne ha verificato le correzioni: cursor serializzato con `BEGIN IMMEDIATE`, configurazione normalizzata/rileggibile prima dell'apply, campi bounded verificati prima della serializzazione. I rispettivi test di regressione sono nel repository.

Verifica integrata e limiti in `docs/verification/VERIFICATION_V0.2.md`. Hardware: `hardware/usb_power/HW02_REVIEW.md`. La proposta storica `docs/protocols/OWNERSHIP_PROPOSAL.md` è stata superata da DEC-011: non serve trasferimento di ownership per passare a un altro telefono. La policy corrente è `docs/protocols/CONNECTION_POLICY_V0.1.md`. Reset fisico USB/pulsanti approvato come manutenzione distinta, non come requisito per nuovi client.

## Ordine successivo

1. Implementare DEC-011: slot di connessione unico, più client sequenziali e reset locale separato. Nessun owner unico persistente; non attivare mutazioni BLE fino a sessione autenticata verificata.
2. Integrare il riferimento log nel core Swift e nella schermata storici con fixture, conservando le garanzie ACK/resume; aggiungere produttore pagine bounded sul firmware.
3. Estendere decoder/ISO-TP con fonti e simulatori, includendo timeout/discovery e addressing necessari; costruire con ESP-IDF qualificato prima di driver reali.
4. Simulare e dimensionare il preregolatore USB candidato e confrontare le alternative wake A/B con OCP/fusibile/SOA e budget corrente completo. Nessun bypass o flag ERC al posto della protezione.

## Terzo incremento completato — DEC-011

| Filone | Risultato | Verifica e limiti |
|---|---|---|
| `app_ios` | Storici da pagine demo realmente importate in archivio locale; deduplica, resume, perdite e ACK dopo scrittura. Testi corretti a più client/una connessione | 32 test Swift, inclusi8scenari log comuni e riapertura dopo ogni operazione; build iOS simulatore. Nessun BLE/cloud reale |
| `firmware` | Slot connessione unico con più client sequenziali, lease contro callback tardivi, reset locale separato con journal e callback simulati | 3 suite CTest con ASan/UBSan; 18 punti d'interruzione I/O. Nessun owner unico e nessun reset per A→B→A; HAL radio/erase ancora inattiva |
| `review_hardware` | Schema KiCad standalone del preregolatore USB H3,792V,14simboli, distinta/matrice/netlist/ERC e confronto3,8–4,5V | Studio isolato non collegato ad A0; ERC0del solo blocco logico. Protezioni/CC, startup, dropout e sourcing ancora aperti |
| Coordinatore | DEC-011 e tracciabilitàv0.2.6, policy sessioni/reset,8fixture log, sorgente volatile con cursori client indipendenti | 30 test Python e verifica integrata. ACKdiA non rimuove dati perB; nessun limite Ndi prodotto introdotto |

Risultati e limiti in `docs/verification/VERIFICATION_V0.3.md`; studio hardware in `hardware/usb_power/design_v01/DESIGN_REVIEW.md`. La proposta storica owner-unico non è più applicabile. Per la policy BLE non resta da chiedere una decisione: è già fissata da DEC-011.

## Quarto incremento e dipendenze attuali

| Filone | Risultato verificabile | Limite |
|---|---|---|
| APP-04 | Endpoint e transport framed condivisi fra config/storici, lease A/B/A, ricevuto/applicato distinti, perdita ACK e rilettura; refactor SwiftUI Pro e rendering reali | 42 test Swift prima di CloudKit, build simulatore; nessun BLE reale |
| FW-04 | Pagine storici bounded fino a16 record,64 record volatile H, raw fino a64byte, ACK non distruttivo | 6 fixture /12 pagine C/Python, due client SQLite; nessuna partizione log flash |
| FW-05 | Due settori4KiB simulati, CRC/marker finale, restore/revisioni/capability separati | 6 suite CTest con ASan/UBSan;24.653 interruzioni simulate, nessun driver flash reale |
| Target ESP32-S3 | ESP-IDFv6.1:11 sorgenti core compilati, binario170624byte | Skeleton inerte, linker scarta core non invocato; niente sizing finale OTA/PSRAM |
| HW-04 | Studio KiCad separato presence sensing,35 pin verificati, ERC0 | Leakage/soglie/temp/standby non qualificati; A0 invariato |
| Integrazione |35 test Python,10 scambi RPC comuni,6 fixture di paginazione, controlli host CI | Non certifica hardware o sicurezza radio |

DEC-013 approva Team ZDQFU74347, bundle `it.ale1x.autogauge` e container `iCloud.it.ale1x.autogauge`. Il tentativo automatico di provisioning Development fallisce per account team non disponibile in Xcode; il proprietario ha chiarito di non avere una membership Developer attiva, richiesta da Apple per CloudKit anche in sviluppo. Nessun abbonamento attivato. Lo sviluppo del codice CloudKit prosegue senza dichiarare una sync reale riuscita. Nessuna pubblicazione App Store.

La review indipendente [FW05_REVIEW.md](../reviews/software/FW05_REVIEW.md) non ha trovato nuovi P0/P1 riproducibili nella persistenza; esplicita letture degradate, stack e limiti del simulatore. [READINESS_AUDIT.md](../reviews/software/READINESS_AUDIT.md) resta uno snapshot storico, da leggere insieme alle verifiche successive: non sostituisce i gate elettrici, meccanici e su veicolo.

APP-05 conclude adapter CloudKit nativo/upload immutabile e outbox locale: suite totale53testSwift,11cloud con fakeDB. La review separata ha portato alla correzione del callback account obsoleto; nessuna fixture caricata. Membership/profilo mancanti bloccano live; download/restore remoto non implementati.

FW-06 aggiunge FlowControl CTS/overflow host per risposte Service01 con le stesse guardieTX e relativi test: suite totale7/7CTest, build target aggiornata riuscita. Non è un driver o una prova di trasmissione reale. DEC-014 approva la UX nuovi telefoni/codice locale, da implementare in sicurezza senza owner unico o dipendenza cloud. Base corrente v0.2.8.

## Quinto incremento — senza membership Apple

Il proprietario intende valutare l’abbonamento dopo il lavoro eseguibile localmente: nessun nuovo servizio/costo attivato. DEC-014 è applicata nel controller di ammissione; non implica radio qualificata.

| Filone | Risultato implementato | Confine conservato |
|---|---|---|
| APP-06 | Restore CloudKit per account con page+checkpoint atomici; record e loss osservate convivono | Query non snapshot, limiti locali bounded; niente prova live |
| APP-07 | Stream/RPC con deadline/generazioni e adapter CoreBluetooth opt-in | UI nativa solohello, nessuna mutazione privata o prova LESC;89testSwift con replay originale |
| FW-07 | Ammissione locale/passkey per nuovi telefoni; riconosciuti e slot unico | Hook entropy/security/bond fidati; nessun binding radio attivo |
| FW-08 | Storico COW simulato persistente, sessioni preservate, serial globale, overwrite/perdite e ACK non distruttivo | ModelloO(capacità), metà spazio, possibileREAD_ONLY dopo power loss; non backendV1 |
| SH-03 | Parser config/RPC C, risposte tipizzate incluso logs.read e budget envelope; reassemblyC | Codec non è dispatcher/autorizzazione; stack da dimensionare |
| OTA-01 | Verifica offline RSA3072-PSS, contesto hardware/capacità/replay e hash immagine | Nessuna installazione/verifica formatoESP/bootselection/eFuse |

La verifica incrociata ha corretto la richiesta indebita diop nelle risposte legacy config: il nuovo pipefinto lo aggiungeva sempre e mascherava la divergenza col golden. Il replay usa ora tutti10scambi originali senza riscrivere fixture/server; il client mantiene il link soltanto su rifiuti applicativi completi e validi, senza trailingframe/partial. Vedi `docs/reviews/software/BLUETOOTH_REVIEW.md` e `docs/verification/VERIFICATION_V0.5.md`.

Il sourcing hardware è stato ricontrollato con ricerca Gmail mirata12:46UTC: soltanto risposte già registrate, nessun nuovo dato tecnico trovato. Pannello/FPC/rail, protezioni e wake/IGN restano aperti. Repo privato già esistente: gli incrementi si pubblicano con commit/push normali, senza force o AppStore.

Verifica finale del coordinatore:67testPython,89testSwift,9/9CTest e build iOSsim unsigned passati con `scripts/check_software.py`; progetto/risorse rigenerati identici. BuildtargetSDKv6.1 riuscita con14core+4condivise e maininerte. ReviewBluetooth aggiornata dopo l’interopfix:23/23test dedicati, incluse5regressioni golden/negative nuove.

## Sesto incremento — integrazione app/firmware

APP-08 usa il client RPC comune per configurazione/storici demo e verifica il processo C su file con riavvio, A–B–A, import prima dell’ACK e commit ambiguo. Diagnostica tipizzata e revisioni lette prima di nuove apply. Review separata APP08:39/39 test dedicati, nessun rilievo aperto.

L’endpoint C integra stream, ammissione, store e storico bounded. Correzioni del coordinatore: nessun current operativo da slot precedente quando il recente è sporco; nessun tick dal vecchio lease sul peer corrente; fsync obbligatorio prima del ripristino del file host. READ_ONLY può richiedere manutenzione: il recupero automatico dopo interruzione resta aperto.

Studio NimBLE separato con bridge verso il vero endpoint, conferma completa EDONE prima del commit, controllo bond/sicurezza e isolamento callback;15 gruppi di fault injection. SDK compila/linka tutte9 entry point, immagine448880byte e main vuoto. Nessuna esecuzione radio. FW-09 estende a53 PID scalari con fonti, min/max e guardie coerenti, senza promessa di disponibilità su una specifica auto.

Verifica finale: **71 test Python,105 Swift,11/11 CTest**, build iOSsim e genericdevice unsigned riuscite. SDK principale15core+4condivise e studio separato compilati; hash sorgenti studio aggiornati, progetto+3risorse rigenerati identici, checkK0/diff puliti. Incrementi firmati e pubblicati come e9cb238 e a6ed648, CI host verde. Vedi [VERIFICATION_V0.6.md](../verification/VERIFICATION_V0.6.md).

Stato prodotto senza hardware: **step4 integrazione operativa ancora in corso**, catena host verificata ma radio/iPhone reali da qualificare. Restano runtime autonomo, persistenza efficiente/recupero, diagnostica restante, OTA/recovery target, CloudKit live dopo membership e QA. Nessun costo o provisioning attivato.

## Settimo incremento — storico append-only e analisi mobile

FW-10 introduce journal a settori con scrittura ordinaria136byte, prenotazioni e witness duplicati contro rollback ambiguo dopo perdita di dati già confermati. Footer di eviction prima dell’erase, contatori/SID monotoni, ACK non distruttivi. Adapter tipizzato per history_read dell’endpoint, senza inizializzazione o writer impliciti. Otto gruppi journal,2573tagliIO principali più bootstrap,8192posizioni-byte XOR1; test golden12pagine e oracolo480operazioni ora nel repository. Review separata senza rilievi concreti aperti. Non è backendV1 qualificato: circa51minuti/5Hz/2MiB, query O(capacità) per pagina, casi ambigui READ_ONLY/manutenzione.

APP-09 aggiunge grafici nativi da archivi realmente importati, filtri sessione/PID/ECU,7formule qualificate e tabella raw/qualità. Massimo240punti originali, no linea attraverso gap/errori, pagine50righe; UInt64 preservati e origine sottratta prima del grafico. Dodici nuovi test core e render del simulatore, senza nuove operazioni BLE/cloud.

OTA-02 aggiunge preflight strutturale ESP32-S3 e API/CLI con firma del manifesto e formato sulla stessa snapshot immutabile;12test e review separata. Non è un installer o una verifica Secure Boot. Nello studio BLE corretti CONNECT rifiutato non tracciato e guasti advertising silenziosi;12gruppi transport+5bridge con sanitizzatori.

Verifica integrata del coordinatore: **83testPython,117Swift,14/14CTest e build iOSsim unsigned riusciti**. SDKprincipale17core+4condivise; studioNimBLE449088byte con9entry point linkate e maininerte, nessuna radio. Misurati frame stack statici journal/adapter, senza dichiarare task stack sufficiente. [Risultati e limiti](../verification/VERIFICATION_V0.7.md). Restano runtime/flash/BLE/recovery fisici, diagnostica restante, OTA target, CloudKit live e QA: stato prodotto ancora integrazione, nessun nuovo costo o provisioning.

## Ottavo incremento — acquisizione autonoma e installer SDK isolato

FW-11 coordina scheduler, qualità e registrazione senza telefono, con job/capability coerenti e budgetglobale1Hz/PID attraverso piùECU. Queue16/drop-newest, BUSY senza mutazione, terminalfault dopo commit ambiguo senza retry; sessione fresca esplicita, nessuna formattazione automatica. Sei gruppi unitari e3indipendenti con20revoche request/FC: wrapper non concede handoff dopo perdita di policy. Runtime ancora non chiamato dalmain e senza driver fisico. Review `docs/reviews/software/RUNTIME_REVIEW.md` senza rilievi concreti aperti.

APP-10 porta a53formule scalari i grafici e il catalogo storico, mantenendo distinta la disponibilità reale della misura. Oracle con212vettori esegue il C vero con sanitizzatori e confronta Swift readonly; freshness verificata automaticamente. Nessun aumento delle7capabilitysimulate o attivazione BLE/cloud. Review `docs/reviews/software/APP10_REVIEW.md` e121testSwift verdi, buildsim/device unsigned riuscite.

OTA-03 è uno studio SDK isolato per write/end/hash reale dello slot inattivo e select separata/ambigua. Verificatore crittografico target obbligatorio ancora da implementare: non è una release OTA. Corretto SDKbegin conhandleallocato prima di eraseerror; cleanupincerto e rifiuti preammissione gestiti esplicitamente. Due review separate,8gruppi host con SDK/hashmodel dichiarati e vera buildSDK con7entry point linkate, mainvuoto e immagine196688byte. Non eseguita alcuna scrittura reale, bootselection, salute/rollback, eFuse o provisioning.

Verifica integrata: **85testPython,121Swift,16/16CTest**, build iOSsim unsigned e crossbuild principale18core+4condivise riusciti. Dopo l’ultimo hardening OTA ripetuti test e SDKbuild interessati, passati. [VERIFICATION_V0.8.md](../verification/VERIFICATION_V0.8.md) esplicita che autonomia fisica, trasferimento OTA/verificatore target, recovery, BLE/iPhone e CloudKit live restano aperti. Il journal mantiene formato/retention precedenti; nessuna promessa nuova di durata o qualifica hardware.

## Nono incremento — verifica OTA interoperabile

APP-11 verifica offline un pacchetto firmato con Security.framework/CryptoKit e restituisce uno snapshot immutabile di envelope/immagine. Parser chiuso e DER PKCS#1 strict, trust locale RSA3072-PSS-SHA256, floor UInt64 e profilo ESP espliciti. Nessun updater/UI/download o nuovo gate BLE/cloud. Review separata: [OTA_PACKAGE_SWIFT_REVIEW.md](../reviews/software/OTA_PACKAGE_SWIFT_REVIEW.md).

FW-12 verifica il manifesto con PSA reale, output transazionale e cleanup fail-closed; wrapper disponibile ma non collegato a main/platform owner o servizio OTA. Studio SDK con9entry point trattenute, immagine254160byte e main vuoto. Nessuna chiave di prodotto o floor durabile provisionato. Fixture condivise:46casi pubblici con chiavi OpenSSL effimere eliminate prima del return;55vettori C inclusi i casi review.

Verifica integrata già eseguita:89testPython,130Swift,16/16CTest e build iOSsim unsigned; genericdevice unsigned riuscita. Prova reale portable TF-PSA-Crypto:55vettori/13manifesti accettati, distinta dallo shim comportamentale e dall’esecuzione ESP32. Chiusura riproducibilità cache e dettagli finali in [VERIFICATION_V0.9.md](../verification/VERIFICATION_V0.9.md). OTA trasferimento/boot health/floor atomico/rollback/recovery, runtime fisico/TWAI/BLE/flash e CloudKit live restano aperti.

## Decimo incremento — trasferimento OTA bounded

APP-12 aggiunge sender Swift actor su trasporto a payload completo, chiuso per default e non collegato a BLE/UI. Usa soltanto candidato immutabile verificato; upload termina READY, select esplicita distinta. ACK esatti per offset/ID/stato, generazione/deadline/cancellazione e busy fra await; ogni esito incerto chiude il sender senza retry o restart. **143testSwift**, inclusi2interop con processoC, buildsim/device unsigned riuscite.

FW-13 aggiunge codec e owner bounded con manifesto4096byte, chunk1024, un trasferimento/boot, lease locale isolata, controlli pre/post callback, polling/deadline e cleanup singolo. Adapter studio collega owner→verificatorePSA→installerSDK. Corretto mapping cancel DENIED+FAULT senza perdere cleanup incerto; select tentata conserva possibile modifica boot e non consente retry.6gruppiC+3review indipendenti,61vettori comuni codec e harness adapter con backend esplicitamente comportamentale.98testPython e17/17CTest superati.

Crossbuild studioSDK reale:12entry point forzate più dipendenze controller,256912byte, mainvuoto e nessuna radio/flash. Progetto/risorse rigenerati identici; review e [VERIFICATION_V0.10.md](../verification/VERIFICATION_V0.10.md) esplicitano che non esistono ancora routeBLE/RPC, provisioning/trust produzione, salute/floor/rollback/recovery o qualifiche fisiche.

## Undicesimo incremento — sessione OTA con frame AG

APP-13 implementa `OTAPacketTransport` su GaugePacketPipe: sessione esclusiva e gatehost chiuso, open senza scan/pairing, frammentazione al limite effettivo≤512, ID consecutivi e singolo ACK esatto. Controlli epoch dopo ogni await, generazione/write-limit, timestamp, timeout frame/operazione; errore terminale disconnette senza riaprire o ritentare. **153testSwift** inclusi8nuovi e2interop framedC, buildsim/device unsigned riuscite.

FW-14 aggiunge endpoint AG con feed che stage soltanto, step separato, leaseobsolete senza callback, replay/trailing/concat/headeroversize respinti prima di dispatch e timer pending assoluto. Close cancella il proprio owner attivo una volta e non lo resetta; select già tentata conserva l’incertezza. Aggregate adapterSDK collega frame→owner→verificatore→installer.6gruppiC+3review indipendenti, harness SDK comportamentale e review senza rilievi aperti.

**100testPython e18/18CTest** passati. CrossbuildSDK con18entry point forzate,260608byte, maininerte e nessuna radio/flash. Sessione OTA non instradata nel bridgeBLE/RPC né abilitata nella UI. Provisioning/health/floor atomico/rollback/recovery e qualifica fisica restano aperti. [VERIFICATION_V0.11.md](../verification/VERIFICATION_V0.11.md).

## Dodicesimo incremento — chiusura dei filoni avviati

Boot/floor/health con NVS e witness locale, factorybootstrap esplicito off perdefault, adapterpartizioni reale, discovery53PID/default, gaugeUI portabile e TWAI moderno con completion; runtime opt-in live-only dopo faultstorage senza retry. Catalogo sessions RPC+scannerjournal ordinato/read-only e source-aware sessioni app. APP14 importa/verifica/salva/riapre pacchetti OTA con trustlocale; APP15 inietta sessioni native soltanto con providerlocale, di defaultdenied, e predispone receipt/inboxcloud distinti dalle fixture.

Chiusura:107Python,175Swift,23/23CTest passati; sim/deviceunsigned e4SDKcrossbuild riusciti. Cryptoportable reale ripetuta55vettori/13accettati. Corrette witnessmask, bindnamespaceimplicito, restore stale/cancelordering, connessioneA che poteva toccareB e timeoutTWAI a partire dall’enqueue. I nuovi percorsi non sono una qualifica fisica o un’applicazione main completa. [VERIFICATION_V0.12](../verification/VERIFICATION_V0.12.md), [backlog aggiornato](../software/SOFTWARE_COMPLETION.md).

## V0.13 — integrazione dopo l'handoff, 25 ottobre 2026

Nuova autorizzazione owner: proseguire e completare autonomamente il software
mentre è assente. Workstream mobile: APP15 intenti durabili/recovery+catalogoC e
APP16 flusso OTA UI/service; firmware: owner/main e TWAI lifecycle; bridge/display:
routeRPC/OTA, tick host e renderer; coordinatore: provisioning/reset NVS,
composizione native, review/fix, tracciabilità e bundle riproducibile.

Suite corrente109Python/25CTest/191Swift, tre profili SDK e build iOS unsigned;
riepilogo e limiti in [VERIFICATION_V0.13](../verification/VERIFICATION_V0.13.md).
Il target native collega insieme gli adapter reali ma rifiuta prima dell'IO senza
profilo locale. Non sono qualificate schede, trust, cache credenziali, cloud o
prestazioni/consumi fisici;53PIDscalari non chiudono PR-003. Nessun test fisico,
flash, acquisto o nuovo messaggio ai fornitori.
