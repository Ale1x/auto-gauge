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

## Connettori e standby — 2026-10-24

| ID | Fonte primaria / canale commerciale | Limite |
|---|---|---|
| E24 | [Molex0430200600](https://www.molex.com/en-us/products/part-detail/430200600), [0430250600](https://www.molex.com/en-us/products/part-detail/0430250600), [specifica famiglia](https://www.molex.com/content/dam/molex/molex-dot-com/products/automated/en-us/productspecificationpdf/430/43045/PS-43045-001.pdf?inline=) | Coppia6vie candidata; contatti/crimp e sistema da verificare. |
| E25 | [Drawing Molex43020](https://www.molex.com/content/dam/molex/molex-dot-com/products/automated/en-us/salesdrawingpdf/430/43020/430200601_sd.pdf?inline=) | Risorsa originale individuata; download locale fallito, quote non trasferite nel CAD. |
| E26 | [Molex0430450400](https://www.molex.com/en-us/products/part-detail/0430450400?display=pdf) | Alternativa PCB4vie; non equivalente al6vie. |
| E27 | [DigiKey0430200600](https://www.digikey.com/en/products/detail/molex/0430200600/252492) | Indicizzati3769 pezzi, USD0,40 qty1 solo housing; non riservati. |
| E28 | [DigiKey0430250600](https://www.digikey.com/en/products/detail/molex/0430250600/252498) | Indicizzati85648 pezzi, USD0,51 qty1 solo housing; non riservati. |

E03 ed E05 ora conservati anche in PDF locale; tabelle di corrente p.6/p.7 renderizzate e ispezionate. E04 §5.5 consultato online per corrente shutdown: condizioni tabellari VIN24V, non misura a12V.

## Pannello LX e recovery USB — 2026-10-24

| ID | Fonte | Esito / limite |
|---|---|---|
| E29 | [LX datasheet originale Ver1.0](https://cdn.ozdisan.com/public/product/assets/LX175A3101CTP01.pdf), [produttore LX](https://www.lxdisplay.com/Product_details/1349774513965064192.html) | PDF conservato, tavole ispezionate. Incongruenze rail; disponibilità non confermata. |
| E30 | [TI TPS2116 Rev.A](https://www.ti.com/lit/ds/symlink/tps2116.pdf) | PDF conservato, tabella limiti/current p.4/5 ispezionata. Recommended ambient+105°C nelPDF, valore pagina commerciale diverso: non estendere autonomamente. |
| E31 | [GCT USB4105](https://gct.co/connector/usb4105) | Famiglia candidata, opzioniMPN e drawing completo da congelare; stock non verificato. |

Ricezione moduli VIEWE/Huaxin confermata nel browser e screenshot conservati in evidence/contact/. Non è conferma di identità tecnica, stock o preventivo.

| E32 | [Huaxin — Janie, risposta24/10/2026](https://mail.google.com/mail/u/?authuser=passarelli598%40gmail.com#all/1a16d3a6a89c9b04) | Campioni a stock dichiarati, USD10/unità; indirizzo richiesto per trasporto. Allegato locale huaxin-email-2026-10-24.pdf: V0,2024-10-11; sola differenza testuale rispetto a E01: LCD→OLED in copertina. Quantità5, identità touch/codice e rettifiche tecniche ancora aperte. |
| E33 | [Ecer — Jessica, richiesta24/10/2026](https://mail.google.com/mail/u/?authuser=passarelli598%40gmail.com#all/1a16d68499259a70) | Buyer Service chiede quantità/applicazione/requisiti/budget/telefono. Nessuna offerta, stock, allegato o conferma tecnica. |

| E34 | [ESP-IDF — Partition tables, ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/partition-tables.html) | Consultato24/10/2026; allineamenti app/dati, esempio otadata. Versione SDK del prodotto non fissata. Modello numerico in CAPACITY_K0.md non è una partition table. |
| E35 | [ESP-IDF — Flash concurrency, ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/spi_flash/spi_flash_concurrency.html) | Consultato24/10/2026; effetti cache/task e restrizioni durante operazioni flash. Funzioni dipendenti da versione/configurazione; nessuna garanzia UI/CAN dal solo task logger a bassa priorità. |

Riverifica contatti E10/E11 il24/10/2026: [JLCPCB support](https://jlcpcb.com/help/article/how-do-i-get-support) indica support@jlcpcb.com; [PCBWay contact](https://www.pcbway.com/contact) indica service@pcbway.com. Usati per richieste di fattibilità, senza ordine.

## Terminali e alternativa rimovibile — 2026-10-24

| ID | Fonte | Evidenza e limite |
|---|---|---|
| E36 | [Molex0430310007](https://www.molex.com/en-us/products/part-detail/430310007) | Terminale maschio sciolto, stagno,20–24AWG,30cicli; compatibilità del cablaggio da verificare. |
| E37 | [Molex0430300007](https://www.molex.com/en-us/products/part-detail/430300007), [tabella43030](https://www.molex.com/en-us/products/series-chart/43030) | Terminale femmina sciolto, stagno,20–24AWG,30cicli; non approvazione del prodotto. |
| E38 | [Molex ATS-638190000 Rev.G](https://www.molex.com/content/dam/molex/molex-dot-com/products/automated/en-us/applicationtoolingspecificationpdf/638/63819/ATS-638190000-001.pdf?inline=) | Documento ufficiale: attrezzo63819-0000, campo filo/isolamento/spellatura e tabelle crimp. Consultato online; revisione/processo effettivo da confermare. |
| E39 | [DigiKey terminali Molex](https://www.digikey.com/en/products/filter/rectangular-connectors/rectangular-connector-contacts/331?s=N4Ig7CBcoIYE5QIwA5EGYA0IYBcmZAAcBLKAJgE4BWMxRMtAX0aA) | Stock/prezzi indicizzati per0430300007/0430310007; nessuna riserva né prezzo consegnatoUE. |
| E40 | [Hirose HR30-6R-6P(71)](https://www.hirose.com/en/product/p/CL0130-1009-0-71), [HR30-6P-6S(31)](https://www.hirose.com/en/product/p/CL0130-0010-4-31) | Alternativa6vie; pagina plug1000cicli,2A,-25…+85°C e finger protection NO. Coppia da verificare con disegni e condizioni d’uso; nessun fit o pinout scelto. |
| E41 | [Mouser presaHR30](https://www.mouser.com/ProductDetail/Hirose-Connector/HR30-6R-6P71?qs=Ux3WWAnHpjBYziFM5okEvQ%3D%3D), [plugHR30](https://www.mouser.com/en/ProductDetail/Hirose-Connector/HR30-6P-6S31?qs=MLItCLRbWsy0nHp6IwfcGw%3D%3D) | Presa1344/USD8,04 indicizzati; plug0/USD15,38 e26settimane. Prezzi non offerta e disponibilità non riservata. |

## Plug OBD e cavo — 2026-10-24

| ID | Fonte | Evidenza e limite |
|---|---|---|
| E42 | [KDS33114 Stecker44](https://kds-online.de/de/obd-2-steckerkit-44-j1962-typ-a-12v-male.html), [categoria](https://kds-online.de/de/obd-zubehoer/), [contatti](https://kds-online.de/de/info/impressum.html) | SKUTypeA12V, kit16contatti, EUR4,99 incl.IVA19% e stato a stock; mancano disegno/rating e conferma lotto5. Contatto pubblico mail@kds-online.de; invio successivamente autorizzato ed eseguito, log separato. |
| E43 | [Molex685031602](https://www.molex.com/en-us/products/part-detail/685031602), [export storico Molex viaTriopak](https://www.triopak.fi/files/product/Molex/685031602.pdf) | Pagina primaria: housing16vie,-10…+85°C e informazioni limitate; export produttore riporta New Business Not Supported. Nessuna conferma di ciclo vita/stock attuale oTypeA completo. |
| E44 | [LAPP UNITRONIC BUS CAN PDF](https://products.lappgroup.com/online-catalogue/data-communication-systems/bus-system-can-devicenet/can-fixed-installation-and-high-flexibility-application/unitronic-bus-can.html?format=pdf) | Cavo2170261 due coppie0,22mm²; impedenza120Ω; nota tensione non per applicazioni potenza, installazione fissa. Nessuna selezione per il gauge. |
| E45 | [KDS cavi open-end](https://kds-online.de/en/obd-accessories/obd-cables/j1962m-open/), [NI193942-02](https://www.ni.com/en/shop/hardware/cables/model-obdii9m-db9f-can-cable) | KDS: variantiTypeA/B, twist/lunghezza da verificare per singolo codice. NI:TypeA2m/no terminazione, D-sub e prezzo daUSD154, fuori target gauge. Nessuna compatibilità vettura o acquisto. |

## Rail MCU e recovery sicura — 2026-10-24

| ID | Fonte | Evidenza e limite |
|---|---|---|
| E46 | [TI TPS6281x-Q1 Rev.K](https://www.ti.com/lit/ds/symlink/tps62813-q1.pdf) | PDF locale tps62813-q1.pdf; famiglia includeTPS62811QWRWYRQ1. Tabella varianti e correnti/limiti consultate, pagina6 renderizzata e ispezionata; non dimensionamento completo. |
| E47 | [TI TPS7A21-Q1 Rev.D](https://www.ti.com/lit/ds/symlink/tps7a21-q1.pdf) | PDF locale tps7a21-q1.pdf; LDO500mA, confronto termico e corrente a carico. Pagina5 renderizzata e ispezionata; variante3,3V non selezionata. |
| E48 | [DigiKey TPS62811QWRWYRQ1](https://www.digikey.com/en/products/detail/texas-instruments/TPS62811QWRWYRQ1/10445028) | Osservazioni indicizzate discordanti per quantità/prezzo: decine di pezzi eUSD2,12–2,47. Riconfermare offertaUE, nessuna riserva né disponibilitàJLC dedotta. |
| E49 | [Espressif USB-Serial/JTAG](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32s3/api-guides/usb-serial-jtag-console.html), [connessione/downloadUSB](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32s3/get-started/establish-serial-connection.html) | GPIO19/20 e accesso manuale al downloader; release prodotto non fissata, nessuna pin matrix congelata. |
| E50 | [Espressif DFU](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/dfu.html), [SecureBootv2](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/secure-boot-v2.html), [FAQ ufficiale](https://documentation.espressif.com/api/resource/path/docs%2Fprojects%2Fesp-faq%2Fen%2Flatest%2Fesp-faq-en-master.pdf) | ROM USB-OTGDFU non disponibile conSecureBoot/flash encryption; distinguereUSBdownload viaflash e policyeFuse. Nessuna recovery sicura già dimostrata. |
| E51 | [TI TUSB320](https://www.ti.com/lit/ds/symlink/tusb320.pdf) | Esempio di rilevamento dei livelli di correnteTypeC. Consultazione documentale, nessun controllerCC selezionato e nessuna conformitàUSB dichiarata. |
