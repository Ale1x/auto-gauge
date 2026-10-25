# Stato corrente — 25 ottobre 2026

Punto di ingresso operativo per le prossime sessioni. Specifica e decisioni restano le fonti dei requisiti; handoff e report versionati conservano lo stato alla data della loro acquisizione.

## App iPhone

App SwiftUI con target iOS 27.0, navigazione Dispositivo / Schermate / Temi / Impostazioni. Aspetto automatico, chiaro e scuro; demo completa con un solo banner e preferenze separate; schermate personalizzabili, luminosità e catalogo delle misure. Le anteprime seguono il layout circolare Waveshare.

Il marketplace offre 60 temi complessivi, in dieci famiglie: originale gratuito, 59 temi a pagamento al prezzo iniziale di 1,99 €, creazione dei propri colori gratuita. Gli acquisti reali restano disabilitati: configurazione App Store Connect e trasporto dei temi al firmware non sono ancora disponibili. I diritti acquisiti nella demo non diventano proprietà reali.

L’onboarding più recente usa tre illustrazioni locali, swipe, azioni native, salto e ripetizione da Impostazioni → Rivedi introduzione. Memorizza il completamento; il replay non cancella dati. Ultime evidenze: [ONBOARDING27](../validation/ios/ONBOARDING27/README.md). Le precedenti revisioni sono in [EXPERIENCE27](../validation/ios/EXPERIENCE27/README.md), [SCREENS27](../validation/ios/SCREENS27/README.md), [THEMES27](../validation/ios/THEMES27/README.md) e [CLUSTER27](../validation/ios/CLUSTER27/README.md).

La build firmata di sviluppo è stata installata sull’iPhone 13 Pro Max; il simulatore iOS 27 è configurato. L’ultima installazione è riuscita, ma l’avvio automatico finale sul telefono era impedito dal blocco schermo. I test UI dell’onboarding sono passati sul simulatore in chiaro/scuro e con testo accessibile massimo. Questo non estende retroattivamente la verifica a ogni flusso o dispositivo.

## Firmware e hardware

Il core C e il target applicativo V0.13 comprendono owner, discovery, runtime, configurazione, journal, renderer RGB565, bridge BLE/OTA e adapter SDK. Default e profilo nativo senza board profile qualificato restano bloccati prima dell’IO; il banco software usa periferiche simulate. La diagnostica comprende 53 decoder scalari, senza dichiarare copertura completa di tutti i PID standard.

Restano aperti board profile e periferiche reali, alimentazione/pannello/touch/ALS/IGN, cancellazione qualificata delle credenziali, trust di produzione e collaudi fisici. Lo schema KiCad A0 e gli studi elettrici non sono una scheda pronta per produzione; K0 non è superato. Vedi [firmware](../../firmware/README.md), [inventario software](../software/SOFTWARE_COMPLETION.md), [gate K0](../hardware/GATE_K0.md) e [blocker](../hardware/issues/K0-001_BLOCKERS.md).

CloudKit è predisposto e testato con database finto; membership, provisioning e prove live restano aperti. Il Personal Team per installazione locale non sostituisce il team/capability CloudKit del template.

## Organizzazione del repository

La documentazione è ora raccolta per argomento in [docs/](../README.md). Codice eseguibile, fixture e percorsi dei progetti Xcode/ESP-IDF restano nei loro moduli. BOM e meccanica sono in `hardware/`; review e risultati UI sono indicizzati nelle rispettive sezioni. La [mappa degli spostamenti](DOCUMENTATION_MAP.json) permette di ritrovare i percorsi citati da vecchie chat, screenshot e report immutabili.

PDF, manifest e documenti hardware legati ad hash sono preservati. `email-response/` resta una cartella locale dell’utente e non entra nei commit o nei pacchetti software.

## Come riprendere

Seguire la richiesta attuale dell’utente e le [istruzioni di sessione](../../AGENTS.md). Per sviluppo app usare la [guida iOS](../mobile/DEVELOPMENT.md); per il banco firmware leggere la [consegna riproducibile](../software/SOFTWARE_DELIVERY.md). Gli snapshot [handoff](../archive/README.md) servono per ricostruire la storia, non per riaprire automaticamente lavori già conclusi o inventare autorizzazioni.
