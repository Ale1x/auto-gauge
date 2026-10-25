# Verifica visiva APP-14

24 ottobre2026. Screenshot dell’app iOS realmente compilata, installata e avviata su iPhone17Pro simulato/iOS26.4, con target minimo iOS17. Il percorso è Dispositivo→Firmware; l’argomento solo Debug `--firmware-preparation` cambia soltanto la navigazione iniziale e non importa file, fornisce trust o attiva radio.

- `firmware-light.png`: titolo/back nativi e banner simulazione; stato «Profilo fidato non configurato», import separato manifesto/immagine e limiti leggibili.
- `firmware-dark.png`: medesimo stato in aspetto scuro dopo il completamento della transizione del simulatore.

La schermata reale non contiene chiavi di prodotto o fixture. La verifica resta disabilitata finché un profilo locale fidato non viene fornito dall’app; selezionare un file non rende fidata una chiave contenuta nel download. I test del core esercitano verifica e riapertura con chiavi OpenSSL pubbliche effimere fornite solo al test.

Ispezionati titolo, back, wrapping, etichette e contrasto visivo dei controlli nel viewport iniziale; contenuti inferiori sono in Form nativa scorrevole. Non eseguiti tap nel file picker, VoiceOver interattivo, DynamicType estremo, salvataggio tramite document provider o prove su iPhone fisico. Light ripristinato dopo la cattura. Non è una qualifica completa di accessibilità o del percorso OTA su hardware.
