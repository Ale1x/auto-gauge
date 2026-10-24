# Verifica design APP-04

Applicata [SwiftUI Pro](../../.agents/skills/swiftui-pro/SKILL.md), con riferimenti design, accessibility, resizability, views, data, API, navigation, localization, performance e hygiene. Target iOS17 conservato, toolchain Xcode27.0/Swift6.4; app compilata in modalità Swift6. Nessuna API27.1 o framework terzo.

## Cambiamenti verificabili per file

- `App/Model/AppModel.swift`, `Root/RootView.swift`: @Observable/@MainActor, binding proiettati e stato di navigazione per scena; nessun forzamento dark globale.
- `App/Navigation/`: stack e destinazioni tipizzate; Tab moderno da iOS18, fallback necessario per iOS17.
- `App/Components/AppActionButton.swift`: label testuale, simbolo e area minima44pt dentro il pulsante. I controlli nativi e le righe mantengono altezza minima44pt.
- `App/Design/AppTheme.swift`, asset colori: font semantici, spaziature condivise, temi light/dark, simboli e testo oltre al colore. `contrast.json` misura accent/warning sui fondi panel/canvas: minimo7,8:1 per i colori sRGB normali esaminati. Non è una certificazione completa di accessibilità.
- `GaugePreviewWidget.swift`: Button accessibile al posto del tap generico, gesto di trascinamento dedicato, @ScaledMetric per i valori di esempio. Ai font di accessibilità l’anteprima geometrica mostra gli ID; nomi/valori di esempio e tutti i controlli restano nella lista con testo dinamico.
- `DeviceContentView.swift`: contenuto condiviso tra schermo scorrevole e rendering; ViewThatFits per le statistiche, nessuna misura UIScreen o geometria basata sul modello di telefono. Gli stati vuoti usano ContentUnavailableView.

## Immagini prodotte e limiti

I quattro PNG sono rendering di SwiftUI ImageRenderer dei componenti reali `SimulationNotice + DeviceContentView`, riutilizzati dallo schermo. Mostrano l’intero contenuto scorrevole alle larghezze320/640pt; non sono screenshot di schermate con barre di navigazione e non includono l’albero UIKit della ScrollView. Il rendering della ScrollView da sola non era supportato correttamente, quindi è stato estratto e renderizzato il contenuto effettivo.

- `narrow-light.png` e `narrow-dark.png`:320pt, DynamicType large, due schemi colore.
- `narrow-accessibility.png`:320pt, DynamicType accessibility3; il contenuto cresce verticalmente e nello schermo resta scorrevole.
- `wide-light.png`:640pt, DynamicType large; statistiche affiancate senza perdere azioni.

Rendering ispezionati come immagini statiche per wrapping, sovrapposizioni e coerenza delle azioni. Testi e controlli si dispongono verticalmente; nessun pulsante essenziale viene nascosto in base alla dimensione. Il debug renderer è attivabile con `--design-snapshots` e scrive nel container locale dell’app, senza radio o rete. I PNG sono artefatti di questa verifica, non input del prodotto.

Restano non eseguiti: collaudo interattivo VoiceOver/VoiceControl, misurazione dei target tramite Accessibility Inspector, gestures/reflow su tutti i runtime, fold iPhoneDuo e barre iOS27.1, tutte le schermate native Form/List. Queste limitazioni non vengono presentate come compliance raggiunta. CUA non espone Simulator.app in questa sessione.

I PNG salvati fotografano APP-04; i nuovi accessi/pagine Bluetooth APP-07 e CloudKit APP-05/06 non sono inclusi in quei render. Usano componenti e controlli nativi coerenti, ma non viene estesa a tali schermate la verifica visiva precedente.

La successiva [verifica APP-09](APP09/README.md) aggiunge screenshot reali del simulatore per grafico, filtri e tabella storici, senza estendere retroattivamente queste evidenze APP-04 alle altre schermate.
