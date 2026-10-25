# Quadrante Waveshare e galleria — 25 ottobre 2026

- Rimosso il titolo fisso Auto Gauge dalla home; le destinazioni mantengono il titolo e il pulsante Indietro.
- Il contenitore TabView occupa nuovamente lo schermo intero. Il banner usa la propria altezza misurata, riservata nelle singole schede; nessun ritaglio della barra o fascia bianca inferiore aggiunta dal contenitore esterno.
- Schermate è una galleria con anteprime grandi, modifica diretta, menu per layout/eliminazione e Salva nella barra superiore.
- Quadrante derivato dal file del proprietario `auto-firmware/firmware/waveshare-display/components/panel_port/cluster_scene.hpp`: griglia 466×466, bordo scuro, tre accenti, titolo, lettura centrale, divisori e letture secondarie. Font Montserrat Medium originale, hash identico a quello dichiarato dal firmware; licenza SIL OFL inclusa nel bundle.
- I valori e le misure rimangono quelli configurati. BMW identifica l’auto di esempio in demo, AUTO GAUGE le altre sessioni. La geometria resta modificabile e coerente con la configurazione; la migrazione automatica riguarda solo la disposizione demo standard, preservando le posizioni personalizzate.

Verifiche finali: 198 test GaugeCore, 8 test AppModel, 8 test UI su iPhone 13 Pro Max/iOS27 tutti passati. Build firmata, installazione e avvio sul vero iPhone riusciti.

Log: `/tmp/ag-waveshare-core-final.log`, `/tmp/ag-waveshare-model-final.log`, `/tmp/ag-waveshare-final-ui.log`, `/tmp/ag-waveshare-iphone-build.log`. Risultati UI: `/tmp/ag-waveshare-final-ui.xcresult`. Immagini e origine in questa cartella e in `screenshots.json`.
