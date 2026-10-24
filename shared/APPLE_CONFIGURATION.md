# Configurazione Apple approvata per CloudKit

24 ottobre 2026. DEC-012 sceglie account iCloud e CloudKit; DEC-013 approva gli identificatori sotto e la relativa configurazione di sviluppo. Questa scheda non dichiara che le risorse siano già create.

Sul Mac è stata trovata un'identità di firma valida del team **ZDQFU74347**. Il proprietario ha approvato questo team. L’identità locale non prova che il relativo account Developer sia accessibile a Xcode.

Identificatori approvati:

| Campo | Valore approvato |
|---|---|
| Team Apple | `ZDQFU74347` |
| Bundle ID app | `it.ale1x.autogauge` |
| Container CloudKit | `iCloud.it.ale1x.autogauge` |
| Primo ambiente | Development |
| Database | Privato dell'account iCloud dell'utente |

La configurazione necessaria comprende App ID/capability iCloud, container e profilo di sviluppo per l'app. Non si pubblica su App Store, non si accetta un nuovo contratto, non si cambia abbonamento e non si programmano dispositivi. Se Xcode richiede credenziali, consenso o adesione a un servizio non già disponibile, occorrerà l'intervento del proprietario.

Il normale sviluppo e i test con database simulato possono continuare senza tali identificatori. L'app deve mostrare lo stato non configurato, senza inizializzare un container mancante o spacciare test mock per sync CloudKit riuscita. I dati dimostrativi non diventano automaticamente storico reale nel cloud. Un eventuale test live usa un record di prova separato e riconoscibile, senza dati personali del gauge.

## Tentativo operativo del 24 ottobre

Il coordinatore ha generato il template `--cloud-development` in una copia temporanea e ha eseguito `xcodebuild -allowProvisioningUpdates build` per `generic/platform=iOS`. La verifica è fallita: **No Account for Team ZDQFU74347** e nessun profilo Development per `it.ale1x.autogauge`. Nessuna risorsa remota o disponibilità del container è stata confermata. Il progetto di lavoro resta unsigned di default.

Il proprietario ha poi chiarito di essere già loggato ma di non avere un abbonamento Apple Developer. Non si ripete il provisioning finché manca un’iscrizione attiva al programma per il team. Non sono state lette password, accettati contratti, registrati dispositivi o inviati dati al cloud. Un certificato locale valido non sostituisce questa autenticazione.

## Blocco membership confermato dal proprietario

Il login Apple gratuito consente sviluppo locale e prove personali, ma Apple richiede un’iscrizione attiva all’Apple Developer Program per abilitare CloudKit anche in sviluppo: [Enabling CloudKit in Your App](https://developer.apple.com/documentation/cloudkit/enabling-cloudkit-in-your-app), [account personale Xcode](https://help.apple.com/xcode/mac/current/en.lproj/devaf282080a.html). La pubblicazione App Store è un’attività distinta e resta esclusa.

Si completa l’adapter e si verificano i comportamenti con database finto; la build locale predefinita mantiene il cloud disabilitato. Nessun acquisto o abbonamento attivato, nessuna sostituzione automatica del provider scelto in DEC-012. Una futura attivazione richiede membership e provisioning, poi prove live; la sola compilazione non dimostra sync iCloud.

Il proprietario ha chiesto di completare prima tutto il lavoro eseguibile senza membership e valuterà l’abbonamento dopo. Non occorre riproporre adesso l’iscrizione: si procede con implementazioni, simulatori, firma/verifica offline e test; attivazione e collaudo CloudKit live rimangono una fase successiva, che non può essere dichiarata già passata.
