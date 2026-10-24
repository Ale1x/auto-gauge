# Fonti K0 — consultazione 2026-10-24

Le pagine commerciali sono osservazioni pubbliche, anche indicizzate: stock/prezzi non sono riservati né preventivi per 5 unità. Le pagine possono cambiare. Il manifest SHA-256 identifica i PDF conservati, non ne certifica la correttezza.

| ID | Fonte primaria | Uso e limite |
|---|---|---|
| E01 | [Huaxin ESHX0175Y003AMT003 V0](https://www.displayamoled.com/photo/displayamoled/document/88489/ESHX0175Y003AMT003-SPEC.pdf) | PDF conservato; pagine fisiche 4–8, tavola meccanica p.5 e applicazione p.7 ispezionate. Sourcing, prezzo e revisione corretta non confermati. |
| E02 | [ESP32-S3-WROOM-1/1U v1.8](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf) | PDF conservato; tabella varianti: N16R8 ha 16 MB flash, 8 MB PSRAM e temperatura ambiente -40…+65 °C; modulo 18×25,5 mm. Precisazione successiva: il testo ammette fino a+85°C con ECC PSRAM attivo e capacità ridotta1/16; vedere MEMORY_RF_K0.md. |
| E03 | [TI TCAN1043A-Q1 Rev.F](https://www.ti.com/lit/ds/symlink/tcan1043a-q1.pdf) | CAN classico, VIO 1,7–5,5 V, sleep/wake e INH. Wake di bus non significa IGN. |
| E04 | [TI LM5164-Q1 Rev.C](https://www.ti.com/lit/ds/symlink/lm5164-q1.pdf) | Buck 6–100 V, 1 A; rating non dimostra immunità del sistema. |
| E05 | [TI LM74900-Q1](https://www.ti.com/product/LM74900-Q1) e [datasheet](https://www.ti.com/lit/ds/symlink/lm74900-q1.pdf) | Protezione candidata 3–65 V con MOSFET esterni; circa 0,63 mA tipici in esercizio. Verificare variante, modalità e percorso always-on; nessun budget standby chiuso. |
| E06 | [TI OPT3001-Q1 Rev.C](https://www.ti.com/lit/ds/symlink/opt3001-q1.pdf) | VDD 1,6–3,6 V; grade 2 -40…+105 °C. Ottica del case da validare. |
| E07 | [Espressif TWAI](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/twai.html) | Controller CAN classico, ID 11/29 bit e transceiver esterno; non è stack OBD/ISO-TP. Versione stabile consultata: v6.1; fissare release in K1. |
| E08 | [Espressif security](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/security.html) | Supporto secure boot; processo OTA/recovery da progettare e testare. |
| E09 | [ADI AN-1123](https://www.analog.com/en/resources/app-notes/an-1123.html) | Rete CAN e terminazioni; non valida automaticamente uno stub di 2 m. |
| E10 | [JLCPCB servizi personalizzati](https://jlcpcb.com/help/article/jlcpcb-supported-personalized-services) | Saldatura flex OLED, inserzioni manuali, flashing/test soggetti a valutazione. Non equivale ad accettazione dell'assieme specifico. |
| E11 | [PCBWay OEM/box build](https://www.pcbway.com/helpcenter/placingorders/How_to_Place_an_OEM_Order_.html) | Servizio generale e processo di quotazione; mancano offerta e accettazione del progetto. |
| E12 | [Seeed Fusion kitting](https://www.seeedstudio.com/kitting-service.html) | Kitting da 5 set e sourcing; inserimento pannello/FPC e assemblaggio finito da confermare. |
| E13 | [Mouser ESP32-S3-WROOM-1-N16R8](https://www.mouser.com/en/ProductDetail/356-ESP32S3WRM1N16R8) | Risultato indicizzato: 43.365 unità, USD 6,72 a quantità 1; apertura diretta non riuscita. Indicazione, non stock live confermato. |
| E14 | [LCSC C2913202](https://www.lcsc.com/product-detail/wifi%20modules_espressif%20systems_esp32-s3-wroom-1-n16r8_C2913202.html) | Risultato indicizzato: 21.613 unità, prezzo promozionale USD 5,0752 a quantità 1. Non prova disponibilità JLCPCB. |
| E15 | [DigiKey TCAN1043ADRQ1](https://www.digikey.com/en/products/detail/texas-instruments/TCAN1043ADRQ1/18159441) | Pagina consultata e risultato: 1.699 unità, USD 1,71 a quantità 1; da riconfermare al preventivo. |
| E16 | [Amphenol catalogo diagnostic](https://www.amphenol-sine.com/pdf/catalog/Diagnostic_Solution_Catalog_Amphenol.pdf) | Famiglia J1962 A/B: nessun MPN di plug/cavo selezionato. Verificare lato mating, chiave e drawing del singolo articolo. |
| E17 | [SAE J1962_201607](https://saemobilus.sae.org/standards/j1962_201607-diagnostic-connector) | Scope ufficiale; testo completo non acquisito. Pinout iniziale deriva dalla SPEC §2.1; revisione normativa applicabile da fissare. |

Nessuna fonte di devboard è stata usata per assegnare il pinout del pannello. Non è stata dichiarata conformità ISO/SAE/EMC né compatibilità della Polo effettiva.

## Approfondimento del 2026-10-24

| ID | Fonte primaria | Esito osservato / limite |
|---|---|---|
| E18 | [Huaxin pagina prodotto](https://www.displayamoled.com/quality-53455909-1-75-inch-round-amoled-display-466x466-resolution-mipi-qspi-interface-700-nits) | HX invece di ESHX, MOQ1000 e lead time4–6settimane; cinque campioni non confermati. |
| E19 | [VIEWE pannello nudo touch](https://viewedisplay.com/product/1-75-inch-466x466-round-amoled-display-with-touch-screen/) | ALL-UEOL018VG-RA31-A001A, USD14,99 esposto; stock e costo consegnato TBD. |
| E20 | [VIEWE PDF V1.0](https://viewedisplay.com/download/72/display-datasheet/6230/all-ueol018wv-ra31-a001a-v1-0-spec.pdf) | Conservato; difformità identità/codici e dati da chiarire. Nessun pinout trasferito al codice venduto. |
| E21 | [Özdisan LX175A3101CTP01](https://www.ozdisan.com/p/grafik-oledler-892/lx-diplay-lx175a3101ctp01-1574703) | Stock0, MOQ1, lead time4–5settimane; ingombro51,5mm dichiarato. |
| E22 | [Eurocoin LX175A3101CTP01](https://eurocoincomponents.com/products/ART212341/) | Canale di richiesta offerta; nessun prezzo/stock confermato. |
| E23 | [Espressif layout moduli](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/pcb-layout-design.html) | Posizionamento antenna e raccomandazione clearance interna15mm; test sul prodotto richiesti. |

Inventario KiCad via MCP, sola lettura: trovati footprint WROOM-1/1U e simboli TCAN1043xDxQ1/LM5164DDA. Nessun controllo pin-per-pin o scelta definitiva.
