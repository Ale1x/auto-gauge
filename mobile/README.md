# Auto Gauge per iPhone — sviluppo simulato

App SwiftUI in italiano, con configurazione dashboard, anteprima statica, luminosità, catalogo PID e storici. Editor e importer comunicano con un **endpoint device in memoria tramite packet AG reali**, codificati/decodificati in entrambe le direzioni. Non ci sono radio, socket, accesso alla vettura, OTA o upload cloud. La simulazione è sempre indicata nella UI.

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

Schema cloud immutabile1: tipo `AGHistoryV1`, campi `schemaVersion`, `payload` JSON canonico e `digest` SHA256. Ogni payload contiene device/session e un record oppure intervallo di perdita esplicita; sequence/monotonic_ms restano stringhe uint64. Il recordID è SHA256 dell’identità device/session/sequence (o estremi della perdita), separato dal digest contenuto. Il digest rileva incoerenze, non autentica i dati. Una fetch identica o un conflitto di creazione identico confermano l’idempotenza; contenuto diverso blocca la coda senza sovrascrivere il server (`ifServerRecordUnchanged`). Non vengono caricate dashboard/configurazione né date assolute inventate. Questo incremento offre upload e riconciliazione dei singoli record, non download completo o unificazione remota delle sessioni. Le ricevute già confermate non vengono riverificate contro cancellazioni/modifiche cloud successive: il contatore indica conferme passate, non un audit attuale del backup.

Il contesto deriva dall’identificatore utente CloudKit, conservato localmente solo come digest. Un cambio account invalida la generazione e annulla il task/operazione di salvataggio in corso; anche i controlli prima e dopo gli await impediscono ricevute locali per un contesto vecchio. La coda precedente resta separata e si riprende solo dopo nuova verifica di quell’account e azione esplicita. Se un salvataggio remoto riesce ma la risposta o la scrittura locale si perde, il pending viene conservato e il retry verifica il record immutabile: non si presume rollback remoto. Annullare non elimina record già ricevuti dal server. I controlli account pre/post e la cancellazione sono difese del client: il SDK non offre in questa API una precondizione server atomica sull’identità account, quindi resta da verificare sul dispositivo il cambio account durante una richiesta già in volo. L’ACK gauge resta esclusivamente conferma di salvataggio locale e non dipende da iCloud.

Il reset fisico USB è manutenzione separata dal cambio telefono: cancellerà credenziali, configurazione e log locali del gauge, senza cancellare cloud o storici già sincronizzati. Questa app non esegue reset reali.

## Verifiche APP-04

Con Xcode27.0 (27A266a), Swift6.4, SDK iOS Simulator27.0: **42 test Swift superati**, build Debug arm64/x86_64 riuscita. Suite:22 casi configurazione,15 vettori framing,8 scenari storico con riapertura a ogni operazione e10 scambi RPC golden condivisi, oltre ai test di failure/disconnessione/revisione/lease/limiti. Nessun dispositivo fisico o BLE reale testato.

Installazione/avvio locale sul simulatore iPhone17Pro/iOS26.4 riusciti. L’archivio prodotto dal bootstrap del nuovo bundle contiene quattro record della sessione05…, perdita1–2 e cursore6, dopo roundtrip attraverso il simulatore.

La [verifica design](DesignValidation/README.md) descrive rendering statici ImageRenderer light/dark,320/640pt e DynamicType accessibility3, contrasto misurato e limiti. Non è un collaudo interattivo completo di VoiceOver o di tutte le schermate native. CUA non espone Simulator.app in questa sessione. La sola compilazione non è stata presentata come verifica visiva.

## Verifiche APP-05

Gli11 test fakeDB/cloud coprono offline/reopen/resume, idempotenza, conflitti locali/remoti, errore scrittura dopo commit remoto, rifiuto fixture, account change durante await e dopo commit, cancellazione, uint64 oltre2^53/fino aUInt64.max, perdite esplicite, decoder diretto invalido, limiti e reload corrotto. Nessun test contatta Apple. Build simulatore e device unsigned verificano la compilazione; cloud live non verificato per mancanza membership/profilo. Schermata Account usa componenti SwiftUI Pro con controlli44pt, etichette italiane e testo dinamico; verifica VoiceOver su dispositivo resta manuale.
