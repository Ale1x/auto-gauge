# Auto Gauge — revisione dell’esperienza iOS 27

La revisione conserva colori, forme e anteprima circolare, ma organizza le azioni per le persone che usano il display: **Dispositivo, Schermate, Viaggi, Impostazioni**.

## Problemi osservati e correzioni

Le tre schermate `before/` sono state catturate prima delle modifiche, usando la precedente build V0.13 sul simulatore iOS 26.4. Mostravano dettagli di sviluppo (RAM, snapshot, revisioni, PID, preparazione firmware) nelle attività quotidiane.

- Un solo **Salva modifiche** per schermate e luminosità, stato del salvataggio comprensibile, annullamento e ripristino con conferma. I controlli di connessione e l’esito incerto di una scrittura restano effettivi.
- Aspetto **Automatico**, **Chiaro**, **Scuro**, memorizzato tra gli avvii. Automatico segue il dispositivo.
- Una sola indicazione della demo, nel banner superiore con azione Esci. L’interruttore nelle Impostazioni consente di rientrare. Nessuna etichetta dimostrativa ripetuta nei contenuti.
- Demo completa: modifica delle schermate, luminosità, due viaggi con grafici esplorabili, copia e recupero dei viaggi, ricerca/download/installazione di un aggiornamento. Configurazione, backup e versione demo persistono in una cartella separata. Nessuna ricevuta qualificata, caricamento iCloud o aggiornamento fisico viene prodotto dalla demo.
- Viaggi con nomi, durata, misure leggibili e dettagli progressivi. Le date esistono per i contenuti demo; non si ricavano date reali dal solo orologio monotono del dispositivo.
- Impostazioni ridotte alle preferenze utili. Il backup reale non disponibile nella build viene spiegato e disabilitato, senza invitare a un accesso inesistente.
- Ricerca del dispositivo con istruzioni brevi e stati comprensibili. I controlli di qualificazione e di sicurezza restano invariati.
- Preparazione degli aggiornamenti con nomi comuni e percentuali; dettagli crittografici conservati nel modello, fuori dalla UI.

## Criteri applicati

[Apple — Onboarding](https://developer.apple.com/design/human-interface-guidelines/onboarding): impostazioni iniziali utili e istruzioni contestuali durante l’attività.

[Apple — Dark Mode](https://developer.apple.com/design/human-interface-guidelines/dark-mode): colori adattivi e preferenza del sistema come impostazione iniziale; le due scelte esplicite rispondono alla richiesta del proprietario.

[NN/g — Progressive Disclosure](https://www.nngroup.com/articles/progressive-disclosure/): dettagli delle letture e operazioni secondarie esposti quando servono.

[NN/g — Recognition vs. Recall](https://www.nngroup.com/articles/recognition-and-recall/): etichette e azioni riconoscibili, senza richiedere la conoscenza dei protocolli.

SwiftUI nativo, aree di tocco di almeno 44 pt, testo dinamico, valori accessibili dei grafici e alternativa ai trascinamenti tramite controlli di posizione/dimensione.

## Verifica

Le prove UI sono in `mobile/UITests/ConsumerExperienceTests.swift`; usano esclusivamente suite di preferenze e directory isolate. `--uitesting-reset` cancella solo tali dati di prova. Le prove del motore demo sono in `mobile/Tests/GaugeCoreTests/DemoExperienceTests.swift`.

Per ripetere le prove (runtime iOS 27 installato e simulatore iPhone disponibile):

```sh
swift test --package-path mobile
python3 -m unittest discover -s tests -p test_app_model_ux.py
xcodebuild -project mobile/AutoGauge.xcodeproj -scheme AutoGauge -destination 'platform=iOS Simulator,name=Auto Gauge • iPhone 13 Pro Max • iOS 27' CODE_SIGNING_ALLOWED=NO ARCHS=arm64 test
```

Verifica finale del 25 ottobre 2026: **196 test GaugeCore**, **8 test del modello AppModel reale con boundary spies**, **7 prove UI** passati. Le sette prove UI sulla versione finale sono state eseguite in due gruppi (3 + 4), entrambi senza errori, su **iPhone 13 Pro Max, iOS 27.0, build 24A434**. Build Debug unsigned per simulatore e generic device riuscite; `check_k0.py` e `git diff --check` passati.

Quindici immagini finali sono in `after/`, con origine dei test in `screenshots.json`. Le catture attendono il completamento delle transizioni native; ogni schermata demo verifica esplicitamente la presenza e l’accessibilità dell’azione Esci del banner. Il layout esterno mantiene il banner distinto dall’area che scorre.

Immagini principali: [Dispositivo](after/01-device-light.png), [Schermate](after/02-screens-light.png), [Viaggi](after/03-trips-light.png), [Impostazioni chiare](after/04-settings-light.png), [Impostazioni scure](after/05-settings-dark.png), [Grafico](after/09-trip-chart.png), [Testo grande](after/13-device-large-text.png).

Log locali: `/tmp/ag-ux-core-full.log`, `/tmp/ag-appmodel-ux-tests.log`, `/tmp/ag-ux27-banner-fix.log`, `/tmp/ag-ux27-final-remaining.log`, `/tmp/ag-ux27-device.log`. Risultati UI: `/tmp/ag-ux27-banner-fix.xcresult` e `/tmp/ag-ux27-final-remaining.xcresult`.

Il simulatore installato è `C36AC983-5C80-4181-B850-5518082923A8`, denominato `Auto Gauge • iPhone 13 Pro Max • iOS 27`. I test non usano l’iPhone fisico.

Non è una ricerca di usabilità con partecipanti. La demo verifica l’esperienza locale, senza qualificare hardware, collegamento reale, iCloud di produzione o installazione OTA fisica.
