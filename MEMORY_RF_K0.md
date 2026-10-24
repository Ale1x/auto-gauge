# Memoria e RF — approfondimento K0, 2026-10-24

## Rettifica temperatura MCU

E02 distingue il rating tabellare N16R8 (-40…+65 °C) dalla condizione con ECC PSRAM: il testo ammette fino a+85°C attivando ECC, con riduzione1/16 della capacità utilizzabile. Quindi il precedente richiamo a+65°C era incompleto. Per8MiB nominali, capacità utile teorica7,5MiB. Verificare configurazione ESP-IDF, prestazioni ed errata; non dichiarare il prodotto idoneo a+85°C: pannello, alimentazione, sole e temperatura interna restano da qualificare in O-07.

## Calcoli riproducibili, non requisiti approvati

RGB565 a466×466:434.312byte per framebuffer; due buffer868.624byte (circa0,828MiB), oltre a heap, decoder, BLE, stack, log e asset. Una variante R2 con2MiB potrebbe essere valutata ma non è selezionata né verificata per disponibilità. ECC non decide da sola quale modulo usare.

O-14 determina la flash: esempio H di15 misure a5Hz con record medio16byte →1.200byte/s →4,32MB/ora (4,12MiB/ora), senza overhead/indici/checksum e senza compressione. OttoMiB interamente dedicati conterrebbero circa1,94ore; non è spazio già disponibile nel modulo. Non si assume che tutte le15 misure siano aggiornate realmente a5Hz.

Una flash16MiB deve ospitare bootloader/partizioni, configurazione, asset e almeno due immagini OTA. Con esempio H di due slot da4MiB ciascuno rimangono **meno di8MiB** per il resto, non ottoMiB tutti per i log. Serve una build reale per dimensionare gli slot. Nessuna partizione congelata.

Aggiornamento DEC-004: owner ha scelto buffer locale limitato con sync BLE successiva e senza sovraccarico. Frequenza/capacità/retention/loss policy da fissare; proposta in LOGGING_K0.md. RTC/timebase e riavvii vanno inclusi nella scelta. Firmware e gauge live restano autonomi dall'iPhone in ogni opzione.

## Vincolo RF nel diametro52mm ipotetico

E23 raccomanda antenna del modulo fuori dal bordo della baseboard o vicino al bordo con opportuni cutout, e distanza libera interna raccomandata di almeno15mm in tutte le direzioni. Non è una quota dimostrata per questo assieme. Vetro, stack metallico del pannello, magnete/supporto e cavo vanno considerati in3D.

L'anello radiale fra48,96 e52mm è1,52mm: non può essere trattato come una zona RF libera da15mm. La profondità e un posizionamento diverso dell'antenna possono cambiare la geometria; perciò non si dichiara impossibilità dell'intero progetto, né fit RF superato. Alternative candidate: WROOM-1U con antenna separata nel case o antenna PCB con volume RF dedicato; nessuna è selezionata. Sono necessari modello3D e misure BLE con display/OTA/cavo e supporto magnetico installati.

## Verifica librerie KiCad MCP (sola lettura)

La ricerca locale ha trovato RF_Module:ESP32-S3-WROOM-1 e RF_Module:ESP32-S3-WROOM-1U; simboli Interface_CAN_LIN:TCAN1043xDxQ1 e Regulator_Switching:LM5164DDA. È un inventario, non una validazione di pin, land pattern o compatibilità della variante automotive. Nessun progetto creato. Il server ha restituito anche SN65HVD257D per la ricerca TCAN: non usarlo come sostituto per somiglianza della ricerca.
