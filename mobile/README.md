# Auto Gauge per iPhone — sviluppo simulato

App SwiftUI in italiano, con configurazione dashboard, anteprima statica, luminosità, catalogo PID e storici. Editor e importer comunicano con un **endpoint device in memoria tramite packet AG reali**, codificati/decodificati in entrambe le direzioni. Il flusso demo non usa radio, socket, vettura o OTA. Le sezioni Bluetooth di sviluppo e CloudKit sono separate e opt-in; il default resta locale/simulato e non carica fixture. Adapter compilati non equivalgono a radio, sicurezza o cloud qualificati. La simulazione del device è sempre indicata nella UI.

## Avvio e verifica

Dalla radice del repository:

```sh
python3 mobile/scripts/generate_project.py
swift test --package-path mobile
xcodebuild -project mobile/AutoGauge.xcodeproj -scheme AutoGauge -sdk iphonesimulator -configuration Debug -derivedDataPath mobile/DerivedData build CODE_SIGNING_ALLOWED=NO
```

Aprire `mobile/AutoGauge.xcodeproj`, schema condiviso `AutoGauge`, simulatore iPhone. Il generatore usa soltanto Python standard, scopre i file Swift per feature, configura gli asset e rigenera le risorse demo dalle fixture condivise. Non serve XcodeGen.

Bundle approvato: `it.ale1x.autogauge`. La configurazione predefinita resta senza firma e senza entitlement cloud. Team/container approvati e opzione esplicita `--cloud-development` sono descritti in [Config/README.md](Config/README.md). Il flag prepara il progetto, non esegue provisioning né avvia CloudKit. Non sono state modificate impostazioni dell’account Apple o pubblicato nulla.

## App e stato

- `App/AutoGaugeApp.swift` è il solo entry point; `App/Model/AppModel.swift` coordina le azioni con @Observable/@MainActor.
- `App/Features/` separa dispositivo, dashboard, impostazioni e storico; `App/Navigation/` mantiene destinazioni tipizzate e stato per scena.
- `App/Design/` e `App/Components/` raccolgono theme, colori adattivi e azioni con area minima44pt. L’app segue l’aspetto del sistema, senza forzare dark mode.
- `Sources/GaugeCore/` contiene configurazione/editor, framing, archivio storico e `Simulation/` con endpoint/canale in memoria. Il package è testabile su host senza SwiftUI.

La UI distingue **tre stati**: bozza di lavoro/salvataggio sul telefono, ultimo snapshot letto dal gauge simulato e applicazione volatile confermata dal simulatore. Leggere il gauge non sovrascrive la bozza; `Usa lo snapshot nella bozza` è esplicito e non salva automaticamente. `Salva bozza sul telefono` non invia alcun comando al device. `Applica al gauge simulato` usa la revisione letta dal device, indipendente dal contatore dei salvataggi locali.

Annulla ripristina l’ultimo salvataggio locale. Ripristina esempio modifica soltanto la bozza. File locale scritto con sostituzione atomica; errori di ricarica non sovrascrivono il salvataggio. La prima apertura può migrare il precedente UserDefaults della simulazione.

## Roundtrip configurazione e connessioni

`SimulatedGaugeTransport` esegue encode richiesta → decode endpoint → encode risposte → decode client per `hello`, `config.get`, `config.validate`, `config.apply`, `logs.read` e `logs.ack`. Verifica kind, requestID, versione, fase e revisione. Le capability del mock sono esattamente sette PID Service01:04/05/0C/0D/0F/10/11; non sono una promessa di supporto di una vettura o di tutti i PID standard.

Applicare richiede `expected_revision` corrente e nuova revisione corrente+1. `received` è distinto da `applied`/`persistence=volatile`. Se il link cade prima del commit, il pending può essere annullato; se il device committa e l’ACK si perde, il client mantiene l’esito **sconosciuto**, conserva la bozza e deve rileggere dopo riconnessione. Nessun rollback o retry cieco e nessuna falsa conferma di persistenza flash.

Il canale consente una sola connessione attiva e lega i pending a lease + requestID. Client A/B/A possono collegarsi in sequenza senza reset; callback del vecchio lease non agiscono sul nuovo. Non c’è owner unico persistente. Token e CRC sono strumenti di simulazione/correlazione, non autenticazione. Il trasporto reale `UnconfiguredGaugeTransport` continua a rifiutare le scritture.

## Editor

Limiti1–3 dashboard e1–5 widget; ID stabili dopo rimozioni, geometrie su griglia intera0–10000. Selezione dalla lista o anteprima, trascinamento, slider e stepper coerenti con gli stessi metodi core. I riquadri che superano il bordo circolare producono un avviso senza cambiare la validazione rettangolare condivisa.

Target **iOS17**, editor libero, font e disposizione sono ipotesi di sviluppo per O-10/O-23. Il modello non dichiara fit finale del pannello. Ai font di accessibilità la visualizzazione geometrica usa gli ID; lista e controlli conservano nomi, valori dimostrativi, selezione e funzioni con testo dinamico. Nessuna dashboard live sull’iPhone.

## Storici

L’app importa pagine dal ring volatile del gauge simulato attraverso `logs.read`, salva prima sul telefono e invia `logs.ack` solo dopo write riuscita. L’ACK non elimina il buffer sorgente per altri telefoni. Ogni installazione ha un proprio cursore device/session; si riparte dal cursore locale per il resume. Il mock iniziale contiene sequenze3–6 e segnala esattamente la perdita1–2 a un telefono con cursore0.

`LogModels.swift` conserva sequenze/tempi uint64 come stringhe decimali canoniche, anche oltre2^53 e fino aUInt64.max, senza Double. Pagine con record/status/raw incoerenti, chiavi duplicate/sconosciute, primitive fuori tipo o gap non dichiarati vengono rifiutate. Gli stati non disponibili mostrano `—`; i validi mostrano bytes raw senza inventare un decoder. Nessuna data assoluta viene ricostruita senza un’ancora affidabile.

`LocalLogArchive` prepara snapshot candidati e li rende attivi soltanto dopo sostituzione atomica del file. Duplicati identici sono idempotenti; conflitti o errori storage preservano lo snapshot e non producono ACK nuovo. Se un ACK verso il simulatore fallisce dopo la write locale, il salvataggio rimane valido e la mancata conferma è dichiarata.

Limiti **del prototipo**, non retention di prodotto/cloud:512KiB di file,1024record complessivi,16sessioni e128intervalli coalescenti; pagina4096byte/max16record/max16intervalli. A pieno si rifiuta la nuova pagina senza eviction. Una sola istanza actor per file, nessun coordinamento fra processi. Atomicità file non significa durabilità a ogni power loss o policy finale di protezione/backup iOS.

Le fixture aggiuntive sono importabili con un pulsante distinto. Le risorse demo derivano da `shared/fixtures/` tramite `generate_demo_logs.py`; non vengono caricate sul cloud.

## iCloud e manutenzione

DEC-012 sceglie account iCloud del telefono e CloudKit senza account email separato. L’adapter nativo APP-05 usa la banca dati privata Development: verifica account e sincronizzazione sono azioni esplicite distinte. Una verifica account riuscita non equivale a backup. Configurazione e blocco Apple Developer Program sono descritti in [Config/README.md](Config/README.md).

La build predefinita unsigned e tutte le build simulatore non costruiscono CKContainer. Il factory reale richiede flag esplicito e un profilo Development incorporato, non scaduto, con Team/app/container/servizio CloudKit approvati; il controllo del template non sostituisce la verifica della firma/provisioning da parte di iOS. I dettagli tecnici Apple restano nella configurazione, fuori dalla UI.

**L’archivio corrente contiene soltanto simulazione.** La UI non lo accoda al cloud, anche su build firmata: `enqueue(..., provenance: .simulation)` viene rifiutato dal core. Non esiste ancora un produttore di storici da radio reale. La sync esplicita può elaborare solo una coda reale già predisposta; senza record reali mostra “Nessuno storico reale”, senza dichiarare backup riuscito. Fixture e test sintetici restano locali/fakeDB.

`CloudHistoryOutbox` salva atomicamente in `cloud-outbox-v1.json`: massimo512 KiB,1024 elementi complessivi e8 contesti account. Il prototipo trattiene anche le ricevute già confermate: al limite rifiuta nuovi inserimenti con errore visibile, senza cancellare pending, archivio o cloud. Nessuna retention cloud è imposta. Un actor possiede un file, senza coordinamento tra processi. Reload/scritture fallite preservano lo snapshot; dopo errore di reload gli inserimenti sono bloccati. Nessun upload automatico/background o migrazione di coda tra account.

Schema cloud immutabile1: tipo `AGHistoryV1`, campi `schemaVersion`, `payload` JSON canonico e `digest` SHA256. Ogni payload contiene device/session e un record oppure intervallo di perdita esplicita; sequence/monotonic_ms restano stringhe uint64. Il recordID è SHA256 dell’identità device/session/sequence (o estremi della perdita), separato dal digest contenuto. Il digest rileva incoerenze, non autentica i dati. Una fetch identica o un conflitto di creazione identico confermano l’idempotenza; contenuto diverso blocca la coda senza sovrascrivere il server (`ifServerRecordUnchanged`). Non vengono caricate dashboard/configurazione né date assolute inventate. APP-05 offre upload e riconciliazione dei singoli record; APP-06 aggiunge il recupero paginato descritto sotto, senza promettere download atomico/completo o unificazione remota delle sessioni. Le ricevute già confermate non vengono riverificate contro cancellazioni/modifiche cloud successive: il contatore indica conferme passate, non un audit attuale del backup.

Il contesto deriva dall’identificatore utente CloudKit, conservato localmente solo come digest. Un cambio account invalida la generazione e annulla il task/operazione di salvataggio in corso; anche i controlli prima e dopo gli await impediscono ricevute locali per un contesto vecchio. La coda precedente resta separata e si riprende solo dopo nuova verifica di quell’account e azione esplicita. Se un salvataggio remoto riesce ma la risposta o la scrittura locale si perde, il pending viene conservato e il retry verifica il record immutabile: non si presume rollback remoto. Annullare non elimina record già ricevuti dal server. I controlli account pre/post e la cancellazione sono difese del client: il SDK non offre in questa API una precondizione server atomica sull’identità account, quindi resta da verificare sul dispositivo il cambio account durante una richiesta già in volo. L’ACK gauge resta esclusivamente conferma di salvataggio locale e non dipende da iCloud.

Il reset fisico USB è manutenzione separata dal cambio telefono: cancellerà credenziali, configurazione e log locali del gauge, senza cancellare cloud o storici già sincronizzati. Questa app non esegue reset reali.

## Verifiche APP-04

Con Xcode27.0 (27A266a), Swift6.4, SDK iOS Simulator27.0: **42 test Swift superati**, build Debug arm64/x86_64 riuscita. Suite:22 casi configurazione,15 vettori framing,8 scenari storico con riapertura a ogni operazione e10 scambi RPC golden condivisi, oltre ai test di failure/disconnessione/revisione/lease/limiti. Nessun dispositivo fisico o BLE reale testato.

Installazione/avvio locale sul simulatore iPhone17Pro/iOS26.4 riusciti. L’archivio prodotto dal bootstrap del nuovo bundle contiene quattro record della sessione05…, perdita1–2 e cursore6, dopo roundtrip attraverso il simulatore.

La [verifica design](DesignValidation/README.md) descrive rendering statici ImageRenderer light/dark,320/640pt e DynamicType accessibility3, contrasto misurato e limiti. Non è un collaudo interattivo completo di VoiceOver o di tutte le schermate native. CUA non espone Simulator.app in questa sessione. La sola compilazione non è stata presentata come verifica visiva.

## Verifiche APP-05

Gli11 test fakeDB/cloud coprono offline/reopen/resume, idempotenza, conflitti locali/remoti, errore scrittura dopo commit remoto, rifiuto fixture, account change durante await e dopo commit, cancellazione, uint64 oltre2^53/fino aUInt64.max, perdite esplicite, decoder diretto invalido, limiti e reload corrotto. Nessun test contatta Apple. Build simulatore e device unsigned verificano la compilazione; cloud live non verificato per mancanza membership/profilo. Schermata Account usa componenti SwiftUI Pro con controlli44pt, etichette italiane e testo dinamico; verifica VoiceOver su dispositivo resta manuale.

## Recupero iCloud APP-06

`CloudHistoryPullDatabase` espone pagine opache account-local; l’adapter usa `CKQueryOperation` su `AGHistoryV1` nella private DB e richiede soltanto schemaVersion/payload/digest. Legge al massimo16 record per pagina; il core accetta payload≤4096 byte, somma payload/identità/digest≤64KiB, cursore opaco≤64KiB e1–8 pagine per azione. Il limite di64KiB riguarda la pagina applicativa, escluso cursore; i callback SDK ricevono comunque un CKRecord prima dei controlli (limiti SDK/servizio distinti). Nessuna API di streaming campi CloudKit viene dichiarata. Cancellazione annulla CKQueryOperation e impedisce checkpoint tardivi.

L’archivio dedicato `cloud-restored-v1.json` mantiene massimo512KiB/1024 elementi/8 account complessivi. Elementi e checkpoint della pagina vengono salvati con una sola write atomica; nessun avanzamento su conflitto, digest/identità errata, errore parziale query o write fallita. Pagine precedenti già scritte restano recuperate anche se una successiva fallisce. Il resume riparte dal cursore persistito di quell’account; la UI richiede verifica account prima di selezionare/rileggere l’archivio, quindi dopo cold launch senza accesso iCloud non espone automaticamente un account precedente. Il pieno blocca nuove pagine senza espellere dati. L’archivio non viene accodato automaticamente all’upload e non invia ACK al gauge.

Il merge differisce dall’import diretto del gauge: una loss è un’**osservazione**, non assenza globale. Un record di B dentro una perdita segnalata da A viene conservato insieme all’intervallo osservato; non è corruzione. Schema1 non conserva l’identificatore del telefono osservatore, quindi non viene inventata una provenienza individuale non disponibile. Intervalli osservati sovrapposti rimangono elementi separati; record device/session/sequence identico si deduplica, contenuto diverso con stessa identità blocca tutta la pagina. Dispositivi/sessioni diversi restano distinti. Il ripristino non forza contiguità né un ACK sequenziale globale, non cancella dati locali e non deduce cancellazioni remote.

Il cursore CKQuery viene archiviato via NSSecureCoding e vincolato al contesto account. Cursor non decodificabile o errore CloudKit invalidArguments/changeTokenExpired durante continuation produce richiesta di nuova scansione, senza restart nascosto o eliminazione dei dati. La nuova scansione è un pulsante esplicito e azzera solo checkpoint/stato di traversata; i record già recuperati restano. Quello che termina è **una scansione**, non una fotografia completa/atomica del backup: [Apple precisa che il cursore non è uno snapshot](https://developer.apple.com/documentation/cloudkit/ckqueryoperation/init(cursor:)) e che [gli indici query si aggiornano in modo asincrono](https://developer.apple.com/documentation/cloudkit/ckqueryoperation/recordmatchedblock-7kek0). Inserimenti/delezioni fra pagine possono causare risultati ripetuti o saltati; ripetere una scansione riconcilia gli elementi trovati ma non dimostra completezza. Non si usano offset sequenziali fabbricati o tempi assoluti come watermark.

Account offre recupero/continua, nuova scansione, cancellazione e navigazione all’elenco cloud separato dagli esempi. I record mostrano stato/raw/tempo relativo; le perdite osservate hanno una spiegazione distinta dalla disponibilità cloud. Il banner globale riguarda il collegamento dispositivo simulato; l’archivio iCloud è etichettato come sorgente separata. Default unsigned/simulatore resta disabilitato e nessuna fixture viene inviata. Nessuna chiamata live/provisioning è stata eseguita. Membership/profilo restano necessari; query/schema/indici non sono stati verificati sul servizio.

Test fakeDB aggiunti: merge loss+record, identità device/session, contatori unsigned64, pagina atomica, offline/reopen/checkpoint, retry deduplicato, conflitti con rollback, digest/ID, cursor scaduto/restart, cambio account durante read/account await, cancellazione, pagine vuote/budget, ripartenza esplicita dopo scan terminata, bounds count/bytes e reload corrotto. I test rimangono locali, senza Apple.

## Bluetooth nativo di sviluppo APP-07

`shared/gatt.profile.json` è copiato deterministicamente in GaugeCore/Resources dal generatore; `GaugeGATTProfile` verifica UUID distinti/versione/tipi interi/limiti e non duplica UUID nel client. `NativeBluetoothController` non costruisce CBCentralManager all’avvio: occorrono toggle sviluppo e pressione esplicita su Avvia ricerca. La descrizione privacy `NSBluetoothAlwaysUsageDescription` è italiana, nella lingua unica dell’app. Nessun test o tool ha attivato la radio, cercato o collegato hardware.

Ricerca filtrata per servizio per massimo10s e20 candidati, nessun connect automatico; un solo peripheral e una sola write/read in attesa. Connect/discovery/CCCD hanno limite30s H. La scrittura usa esclusivamente `.withResponse` e attende callbackATT per ogni chunk≤min(maximumWriteValueLength,512); il canale non è pronto prima della conferma CCCD. La caratteristica risposta deve offrire indicazioni senza proprietà notify concorrente (CoreBluetooth non consente di scegliere arbitrariamente fra entrambe). Nessun MTU o payload20/512 è assunto garantito. Cancel, uscita schermata e background fermano scan/link; nessun background mode, reconnect o retry infinito. Le richieste si annullano alla chiusura. Proxy delegate per lease/generazioni, identità di central/peripheral/caratteristiche e ricreazione manager al nuovo scan impediscono riuso intenzionale di callback/chunk fra collegamenti.

Lo stream AG usa `GaugeStreamReassembler`: un frame incompleto≤4112byte, chunk≤512, header/version/kind/ID/reserved/length e CRC controllati, nessuna ricerca di magic dentro dati corrotti. Timer assoluto10s dal primo byte, non rinnovato dai frammenti. Il nativo valida all’arrivo anche se l’ultimoACKATT è ancora pendente: conserva il solo frame incompleto e mette in coda soltanto frame completi validati (coda separata≤4112byte, poi frammenti≤512 per il pipe). Coda piena/indicazioni non richieste/corruzione chiudono il link senza troncare dati. Il clientRPC ha un ulteriore assembler bounded e deadline assoluta30s per richiesta, compresa la scrittura; i timer scadono anche senza nuovi bytes. Questi sono parametri H, non throughput qualificato.

`GaugeRPCClient` è indipendente da CoreBluetooth e testabile con packetpipe finto: una richiesta in volo, requestID crescente nonzero nella connessione, nessun reset ID su open ripetuta/stessa generazione o wrap. Verifica op/kind/ID/versione/fase/capability, distingue received da applied e legge configurazione soltanto dopo hello/capability valide. La configurazione letta passa il validator con i soli PID dichiarati (ECUunknown, senza inventare supporto numerico). Il riferimento di sviluppo conferma solo persistence=volatile; niente durable simulato. Eventi non gestiti/risposte di altre richieste abortiscono il client invece di essere riusati.

Apply può aver committato già alla dispatch dell’ultimo chunk, anche se l’ACKATT manca: perdita link/timeout/errore produce esito sconosciuto, nessun rollback/retry cieco. Solo revision_conflict/invalid_configuration sono rifiuti precommit noti nel contratto di sviluppo; I/O dopo received resta ambiguo. Con esito sconosciuto una nuova apply è bloccata fino a config.get valido dopo riconnessione/hello. Le risposte già arrivate prima dell’ACKATT finale restano accodate. I test possono abilitare esplicitamente mutazioni su pipe finto (`allowTestMutations`); il percorso nativo mantiene sempre chiuso quel gate e non lo deduce da hello.authenticated o da ATT.

La UI nativa offre solo selezione/connessione/hello di sviluppo con stato **sicurezza non verificata**. Configurazione e log reali non sono collegati all’editor/importer, e ogni mutazione reale resta bloccata finché LESC/MITM/128bit/ammissione e server radio non sono provati. Il telefono non può osservare tutti i flag SMP: hello/CCCD/ATT non autenticano il server. Nessun codice fisso, bond wipe, reset per cambiare telefono o owner unico viene introdotto. Più telefoni sequenziali/una connessione restano DEC-011; iCloud è distinto. Futuro passo hardware: qualificare server/admission/indications e pairing con passkey sul gauge, poi collegare i flussi reali attraverso prove e nuovi gate espliciti.

Test host aggiunti: framing massimo/varie frammentazioni, CRC/header/kind/versione/ID, coalescing e partial, deadline assoluta con drip e senza bytes, limiti/profile condiviso, CCCD/writeLength, una richiesta, cancel/lease vecchi, gate anche con authenticated=true, received/applied primaACKATT, ultimoACKATT perso dopo commit, I/O ambiguo/rifiuto precommit, blocco retry fino a lettura validata. Build sim/device unsigned verificano compilazione, non pairing/radio/sicurezza. PNG DesignValidation sono snapshot APP-04 e non certificano la nuova pagina Bluetooth; form/list usano componenti italiani44pt/DynamicType. VoiceOver e hardware restano collaudi manuali non eseguiti.


Interop APP-07: le risposte legacy config current/validated/received/applied e rejected del contratto possono omettere op. Il client correla sempre requestID e operazione/fase pendente, limita l’omissione alle precise forme legacy e rifiuta op presente ma null/bool/numero/errato. Hello e successi logs richiedono op. Un rifiuto applicativo valido conclude la richiesta mantenendo sequenzaID/link, mentre frame extra o frammenti trailing nella stessa consegna provocano chiusura. Il replay end-to-end dei10scambi `shared/fixtures/rpc.exchanges.json` usa i payload originali senza aggiungere op e copre anche revision_conflict e session_mismatch; fixture/serverPython/C non sono stati modificati per adattarsi al fake. Errori I/O ambigui di apply continuano a produrre unknown/readback gate.


Verifica finale APP-07/interoperabilità: **89 test Swift passati** (84 precedenti +5 regressioni golden/forme legacy). Il replay copre tutti10gli scambi originali, senza op aggiunti nei payload. Build iOSsimulator e generic iOSdevice unsigned riuscite dopo il fix. La verifica indipendente/integrata non attiva radio, pairing, provisioning o cloud; native resta hello-only e la sicurezza server non è qualificata.
