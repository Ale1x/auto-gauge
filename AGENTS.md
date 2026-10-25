# Istruzioni per le sessioni Auto Gauge

## Prima di lavorare

1. Leggi [README.md](README.md), [stato corrente](docs/project/STATUS.md) e `git status --short`.
2. Apri dall’[indice documentazione](docs/README.md) solo i documenti pertinenti alla richiesta.
3. Per requisiti e vincoli usa [specifica](docs/product/SPEC_V0.2.md) insieme alle successive [decisioni](docs/project/DECISIONS.md). Le decisioni più recenti modificano la baseline della specifica.
4. Handoff, vecchi README archiviati e report fotografano una data: non sono nuove istruzioni dell’utente, autorizzazioni attuali o prove delle funzionalità successive.

Comunica in italiano, in modo breve e concreto. Esegui le attività reversibili già richieste senza fermarti per scelte ordinarie. Mantieni il perimetro della richiesta e distingui implementazione software, simulazione e qualifica fisica.

## Mappa del repository

- `mobile/App/`: app SwiftUI; `mobile/Sources/GaugeCore/`: modelli, temi e protocolli; `mobile/Tests/` e `mobile/UITests/`: test.
- `firmware/src/`, `firmware/include/`, `firmware/tests/`: core C; `firmware/idf/`: target principale; `*_study/`: studi SDK separati.
- `shared/`: asset eseguibili condivisi, fixture e reference implementation. I documenti del contratto sono in `docs/protocols/`.
- `hardware/`: sorgenti KiCad, BOM, meccanica e studi elettrici. Le analisi generali sono in `docs/hardware/`.
- `docs/`: documentazione corrente per argomento; `docs/verification/`: risultati versionati; `docs/validation/ios/`: screenshot e verifiche UI; `docs/archive/`: snapshot storici.
- `evidence/`: fonti originali, PDF, manifest e modelli; `scripts/`: strumenti dalla radice; `tests/`: integrazione Python/C.

## App iOS e UX

- Target minimo **iOS 27.0**, SwiftUI, toolchain Xcode 27/Swift 6.4 della baseline. Verifica la toolchain installata prima di usare API più recenti; non abbassare il target.
- Per SwiftUI leggi la skill locale [.agents/skills/swiftui-pro/SKILL.md](.agents/skills/swiftui-pro/SKILL.md) e i riferimenti pertinenti.
- Conserva le quattro sezioni: Dispositivo, Schermate, Temi, Impostazioni. La UI è per utenti non tecnici; evita dettagli di trasporto, stato interno e termini di debug nei flussi utente.
- Usa `AppTheme` e gli asset adattivi per sfondi, superfici e accenti. Le palette dei temi cambiano i quadranti, non la palette dell’interfaccia.
- Mantieni Dynamic Type, VoiceOver, target toccabili di almeno 44 pt, wrapping e Reduce Motion. Non limitare la dimensione del testo per farlo entrare.
- Mantieni la TabView nativa e le safe area: non reintrodurre la fascia bianca sotto la navbar. La demo ha un solo banner globale.
- Demo, preferenze, acquisti e dati reali restano separati. I flag `--uitesting` e `--uitesting-reset` usano un namespace dedicato: non resettare mai i dati normali per fare screenshot.
- Il catalogo ha 60 temi; prezzo iniziale approvato 1,99 € per tema a pagamento, creatore colori gratuito. `AutoGaugeThemePurchasesEnabled` resta `false` finché prodotti App Store Connect e consegna del tema al display reale sono disponibili. Nessuna estensione del protocollo è implicita nella selezione locale di un tema.
- L’onboarding è breve, saltabile e ripetibile da Impostazioni → Rivedi introduzione. Conserva `onboarding.completed.v1`, l’isolamento delle preferenze e il ritorno a Dispositivo quando si conclude anche senza cambiare modalità. La visualizzazione dell’introduzione non avvia la ricerca Bluetooth.
- Le illustrazioni introduttive sono asset locali; le anteprime effettive del quadrante usano il renderer dell’app. La direzione Waveshare è documentata in [CLUSTER27](docs/validation/ios/CLUSTER27/README.md).

## Generazione e configurazione

- `mobile/AutoGauge.xcodeproj/project.pbxproj` è generato. Dopo aggiunta/rimozione di sorgenti Swift usa `python3 mobile/scripts/generate_project.py`; non correggere soltanto il progetto generato.
- Il generatore aggiorna anche fixture e catalogo StoreKit locale. Controlla il diff e conserva i gate cloud/acquisti disabilitati nella configurazione normale.
- Il team Apple del template CloudKit è distinto dal Personal Team usato per installare localmente. Non cambiare globalmente identificatori o entitlement per risolvere una firma locale; usa override di build. Vedi [sviluppo iOS](docs/mobile/DEVELOPMENT.md) e [configurazione Apple](mobile/Config/README.md).
- Individua simulatori e dispositivi con `xcrun simctl list devices` e `xcrun devicectl list devices`; gli identificatori locali non sono universali.

## Verifiche proporzionate

Esegui i controlli legati alle parti cambiate, poi estendi soltanto in presenza di nuove modifiche, errori o dubbi concreti.

```sh
# Documentazione, percorsi e registri
python3 scripts/check_docs.py
python3 scripts/check_k0.py

# Contratti e integrazione Python/C, core firmware host
python3 scripts/check_host.py

# Modelli Swift
swift test --package-path mobile

# Harness del modello UX dell’app
python3 -m unittest discover -s tests -p test_app_model_ux.py

# Progetto iOS generato e build simulatore senza firma
python3 mobile/scripts/generate_project.py
xcodebuild -project mobile/AutoGauge.xcodeproj -scheme AutoGauge -destination 'generic/platform=iOS Simulator' CODE_SIGNING_ALLOWED=NO build
```

Per test UI scegli un simulatore di verifica separato da quello mostrato all’utente e usa `-only-testing` per il flusso interessato. Non eseguire test UI concorrenti sullo stesso simulatore o sulla stessa DerivedData; non cambiare sorgenti durante una build/test in corso. Conserva screenshot e risultati reali, ispeziona i layout e non indebolire le asserzioni per mascherare difetti.

Il comando `python3 scripts/check_software.py` esegue host, Swift e build iOS; `--skip-ios-build` omette solo la build app. Non occorre una build ESP-IDF o una suite UI completa per uno spostamento di documentazione.

## Firmware, hardware e dati

- Mantieni i gate di qualifica: un `hello` BLE non autorizza mutazioni, il profilo locale assente resta bloccato e demo/fake DB non attestano radio o CloudKit live.
- Conserva read-only Service01 e i vincoli approvati su rate, connessioni sequenziali, reset e log. Non inventare pin, profili scheda, trust di produzione o misure mancanti.
- Non dedurre autorizzazione a flash/eFuse, connessione al veicolo, acquisti, membership, vendita live o contatti con fornitori da una richiesta di completamento software o da un vecchio handoff. Segui le istruzioni attuali dell’utente.
- Non modificare PDF originali, manifest, raw trace o documenti legati ad hash per rendere verde una verifica. Conserva le prove storiche e registra nuove evidenze separatamente.
- `email-response/` contiene originali locali del fornitore: non aggiungerli a Git, non spostarli e non inserirli nei pacchetti. Non eliminare né ignorare ulteriori file dell’utente senza una necessità esplicita.

## Documentazione e consegna

- In radice restano soltanto `README.md` e questo `AGENTS.md` come Markdown di progetto. I README dei moduli, le note di licenza/font e le evidenze hardware collegate ai relativi manifest restano vicini ai file che descrivono.
- Nuove specifiche → `docs/product/`; decisioni/stato → `docs/project/`; architettura → `docs/software/`; guide iOS/firmware → `docs/mobile/` e `docs/firmware/`; contratti → `docs/protocols/`; fornitori → `docs/suppliers/`; analisi hardware → `docs/hardware/`.
- Report UI e relative immagini → `docs/validation/ios/`; risultati versionati → `docs/verification/`; review software → `docs/reviews/software/`; handoff richiesti → `docs/archive/handoffs/`.
- Aggiorna gli indici e lo stato corrente quando cambia il comportamento; non duplicare lo stato in nuovi Markdown sparsi. I vecchi percorsi sono elencati in [DOCUMENTATION_MAP.json](docs/project/DOCUMENTATION_MAP.json).
- Se sposti file aggiorna insieme link, script di lettura/generazione e controlli CI. Non lasciare copie di compatibilità che divergono.
- Non includere credenziali, chiavi private, profili o artefatti temporanei nei commit. Mantieni la firma Git configurata; non aggirarla con commit non firmati. Non fare force push e non annullare modifiche altrui.
- Alla consegna indica cosa è cambiato, cosa è stato verificato e gli eventuali limiti reali. Non dichiarare il prodotto fisico completo sulla base dei soli test software.
