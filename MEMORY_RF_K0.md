# Memoria e RF — approfondimento K0, 2026-10-24

## Rettifica temperatura MCU

E02 distingue il rating tabellare N16R8 (-40…+65 °C) dalla condizione con ECC PSRAM: il testo ammette fino a+85°C attivando ECC, con riduzione1/16 della capacità utilizzabile. Quindi il precedente richiamo a+65°C era incompleto. Per8MiB nominali, capacità utile teorica7,5MiB. Verificare configurazione ESP-IDF, prestazioni ed errata; non dichiarare il prodotto idoneo a+85°C: pannello, alimentazione, sole e temperatura interna restano da qualificare in O-07.

## Calcoli riproducibili, non requisiti approvati

RGB565 a466×466:434.312byte per framebuffer; due buffer868.624byte (circa0,828MiB), oltre a heap, decoder, BLE, stack, log e asset. Una variante R2 con2MiB potrebbe essere valutata ma non è selezionata né verificata per disponibilità. ECC non decide da sola quale modulo usare.

O-14 determina la flash: esempio H di15 misure a5Hz con record medio16byte →1.200byte/s →4,32MB/ora (4,12MiB/ora), senza overhead/indici/checksum e senza compressione. OttoMiB interamente dedicati conterrebbero circa1,94ore; non è spazio già disponibile nel modulo. Non si assume che tutte le15 misure siano aggiornate realmente a5Hz.

Una flash16MiB deve ospitare bootloader/partizioni, configurazione, asset e almeno due immagini OTA. Con esempio H di due slot da4MiB ciascuno rimangono **meno di8MiB** per il resto, non ottoMiB tutti per i log. Serve una build reale per dimensionare gli slot. Nessuna partizione congelata.

Aggiornamento DEC-004: owner ha scelto buffer locale limitato con sync BLE successiva e senza sovraccarico. DEC-006 ha poi approvato2MiB condizionatiOTA, fino1Hz/PID e overwrite con segnalazione; retention non garantita e formato da verificare in LOGGING_K0.md/CAPACITY_K0.md. RTC/timebase e riavvii vanno inclusi nella scelta. Firmware e gauge live restano autonomi dall'iPhone in ogni opzione.

## Vincolo RF nel diametro52mm ipotetico

E23 raccomanda antenna del modulo fuori dal bordo della baseboard o vicino al bordo con opportuni cutout, e distanza libera interna raccomandata di almeno15mm in tutte le direzioni. Non è una quota dimostrata per questo assieme. Vetro, stack metallico del pannello, magnete/supporto e cavo vanno considerati in3D.

L'anello radiale fra48,96 e52mm è1,52mm: non può essere trattato come una zona RF libera da15mm. La profondità e un posizionamento diverso dell'antenna possono cambiare la geometria; perciò non si dichiara impossibilità dell'intero progetto, né fit RF superato. Alternative candidate: WROOM-1U con antenna separata nel case o antenna PCB con volume RF dedicato; nessuna è selezionata. Sono necessari modello3D e misure BLE con display/OTA/cavo e supporto magnetico installati.

## Verifica librerie KiCad MCP (sola lettura)

La ricerca locale ha trovato RF_Module:ESP32-S3-WROOM-1 e RF_Module:ESP32-S3-WROOM-1U; simboli Interface_CAN_LIN:TCAN1043xDxQ1 e Regulator_Switching:LM5164DDA. È un inventario, non una validazione di pin, land pattern o compatibilità della variante automotive. Nessun progetto creato. Il server ha restituito anche SN65HVD257D per la ricerca TCAN: non usarlo come sostituto per somiglianza della ricerca.

Approfondimento quantitativo riproducibile in CAPACITY_K0.md e evidence/models/: allocazione OTA ipotetica, overhead/formato log e latenza diagnostica. Non sostituisce build o prove.

## Audit KiCad del transceiver candidato — 2026-10-24

Eseguita lettura MCP `get_symbol_info` e confronto riproducibile della libreria installata con E03, sezione4: scripts/audit_tcan_library.py conserva hash e risultati in evidence/kicad-tcan-library-audit.json. Tutti14 nomi/numero pin del simbolo Interface_CAN_LIN:TCAN1043xDxQ1 corrispondono alla tabella TCAN1043A-Q1, normalizzando le barre attive basse. Sono presenti14pad numerati1–14 nel footprint associato. Questo risultato non seleziona il componente o approva il footprint.

Il simbolo generico punta al datasheet TCAN1043-Q1 senza suffissoA: in un eventuale progetto futuro occorrerà associare l’MPN esatto e E03, senza modificare globalmente la libreria di sistema. NC11 è nascosto nel simbolo: verificarne la gestione in review e non usarlo come segnale. INH7 e WAKE9 sono ad alta tensione; il nome del simbolo e l’ERC non sostituiscono il controllo dei domini e dei GPIO a3,3V. VSUP10, VCC3 e VIO5 sono alimentazioni distinte.

Differenza geometrica da esaminare: footprint generico con pad1,95×0,60mm centrati aX±2,475mm; esempio TI D0014A con pad1,55×0,60mm e distanza fra file5,4mm. Il passo nominale1,27mm e la classeSOIC14 corrispondono, ma i land pattern non sono identici. Non è dimostrata incompatibilità; scegliere il criterio di land pattern e verificare solder mask/paste, tolleranze e DFM prima del freeze. La variante D/SOIC non richiede di importare il thermal pad della diversa variante DMT/VSON. Nessuna libreria modificata e nessuno schematico/PCB creato.
