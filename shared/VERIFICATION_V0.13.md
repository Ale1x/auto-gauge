# V0.13 — integrazione applicativa e consegna software

25 ottobre 2026. L'utente ha chiesto di proseguire autonomamente mentre è assente.
Requisiti v0.2.8, iOS17, un solo client BLE attivo e diagnostica Service01 read-only
restano invariati. Nessun accesso a dispositivo, radio, flash/eFuse, veicolo,
cloud live, acquisto, membership o messaggio a fornitore è stato eseguito.

## Risultato

- APP15: record/cursore/intenti cloud qualificati nello stesso snapshot archivio
  v2; replay verso inbox idempotente, rimozione dopo commit inbox, errore di write
  ambiguo richiede reload. Migrationv1 senza promuovere fixture/vecchi dati.
  Unico actor archivio nativo in AppModel e recovery startup offline. Nuovo test
  catalogo Swift→vero processo C, compresi cursori esclusivi e cambio sessione.
- APP16: fileimport/verifica→upload→READY→select esplicito con conferma; progresso
  derivato dagli ACK, annulla/errore→UNKNOWN senza retry. Controller conservato
  in AppModel fra navigazioni. Trust/provider locali iniettati, assenti nel
  normale avvio; nessun gate aperto da hello o file. Finestre native esclusive
  e lease attesa proteggono anche il trasporto OTA.
- Owner firmware: discovery/config/runtime/journal/UI/RPC/boot nello stesso
  ciclo, reconfigurazione senza nuovo SID o reset della quota per PID, clock
  fresco dopo IO, fault con quiesce/scena sicura una volta. Timeout risposta
  sospeso soltanto durante TXrequest queued; FC non rinnova la deadline.
- Display: renderer RGB565 circolare466px, buffer di una riga932byte, font
  proprio, PID/valore/unità e indisponibilità; zero/negativi/clipping/overflow
  e fault testati. Il profilo nativo completa il frame entro budget locale
  esplicito, senza emettere vecchi snapshot su più tick.
- Provisioning/reset: config blank-only revision1, noerase/overwrite/retry;
  record NVS canonico32byte con probeRO, readback, latch e fasi durabili.
  Recovery del reset interrotto prima di radio/CAN con nuova arm/conferma USB.
  Nessuna creazione automatica di default/trust/floor o cancellazione globale.
- BLE/TWAI: bridge con endpoint/admission condivisi, route RPC/OTA esclusiva,
  autorizzazione corrente, cleanup/UNKNOWN e barrier indicationEDONE prima del
  commit RPC. Tick applicativo sul medesimo task host anche senza telefono;
  endpoint RPC differito fino alla discovery. TWAI quiesce/suspend/resume soltanto
  da stato sano e senza TX pendente; errori SDK restano terminali.
- Target unificato: `native_profile` collega partizioni, reset, TWAI, NimBLE,
  verifier/installer/transfer OTA e boot NVS reali nello stesso binario.
  Getter locale predefinitoNULL: rifiuto prima di prepare/IO/radio. Profilo
  richiede dati elettrici/trust/entropia/salute e callback locali qualificati.
  Bootstrap e host usano stack di sviluppo32KiB, non margini fisici misurati.

## Verifiche

`python3 scripts/check_software.py --skip-ios-build`: suite integrata iniziale
108Python,25/25CTest,191Swift passati. La consegna ripete la suite includendo
il nuovo harness native: **109Python,25/25CTest,191Swift passati**. I test C sono
compilati con ASan/UBSan; nessun errore sanitizer rilevato.

Gruppi pertinenti: config9 (comprese24653frontiere di fault commit), maintenance4,
application10, renderer4, TWAI9, NimBLEtransport13+bridge11; il test native aggiunge
12scenari con produzione native_profile+bridge e intero core reale, soli adapter
SDK comportamentali. APP15 aggiunge6regressioni recovery+1catalogoC e APP16 aggiunge
9casi controller/trasporto rispetto ai175Swift V0.12. Le191prove non qualificano
entropia/RF/flash/controller reali o durabilità fisica del telefono.

Build iOS Simulator e genericdevice unsigned ripetute dal worker con exit0 e
nessun warning; la consegna le ripete e raccoglie i due prodotti `.app`. Progetto
Xcode e tre risorse JSON rigenerati identici. CheckK0/diff puliti.

Crossbuild ESP-IDFv6.1 fissato al commit
`fff9895c82d744c7237be8847347bdd1b07c6643`, senza esecuzione:

| Profilo | Binario | Significato |
|---|---:|---|
| Default |201360byte| Owner chiamato senza boardprofile, resta bloccato |
| Banco |214880byte| Core/renderer/flash/CAN simulati in RAM, nessuna periferica |
| Native unificato |601680byte|168simboli ag_ trattenuti, veri adapter SDK; profiloNULL rifiuta l'avvio |
| Studio BLE |455648byte| Bridge/route reali linkati, main inerte |
| Studio platform |236096byte| Partizioni/NVS/TWAI reali linkati, main inerte |
| Studio OTA |265088byte| Verifier/installer/boot veri linkati, main inerte |

I report `device-build-report.json`, `device-bench-build-report.json` e
`device-native-build-report.json` riportano SHA dei C e degli header di input.
La mappa native4MiB con slot1MiB/log256KiB è di sviluppo, non il sizing V1
né una conferma della memoria delle due ESP32 dell'utente.

Review indipendenti e tre fix native in
[INTEGRATION_V0.13_REVIEW](../reviews/INTEGRATION_V0.13_REVIEW.md).
Audit concreto del limite reset credenziali in
[BLE_CREDENTIAL_RESET_AUDIT](../reviews/BLE_CREDENTIAL_RESET_AUDIT.md).
La CI V0.12 precedentemente non attesa è stata ora verificata: run40182358192,
headc215a19, completed/success; questo non è l'esito CI dei nuovi sorgenti V0.13.

## Consegna e limiti residui

`scripts/build_software_bundle.py` ripete check/build in sequenza, raccoglie
binari/ELF/configurazioni/app unsigned/log e scrive manifest SHA-256 soltanto
alla fine. Output ignorato da Git in `output/software/`, originali fornitori
esclusi. I log della sessione e il manifest della consegna identificano le
verifiche esatte, senza dedurre una qualifica dal solo numero di test.

Rimangono necessari: boardprofile delle due schede e del prodotto, port/protocollo
pannello e touch con livelli/rail/tempi confermati, ALS/IGN e salute quantitativa,
trust/chiavi/rotazione produzione, provider nativo iOS qualificato e lifecycle
che cancelli credenziali persistenti e cache operative preservando reset/floor.
Il callback credenziali manca nel profilo predefinito e il reset viene rifiutato:
`ble_gap_unpair`/`ble_store_clear` non vengono spacciati per purge completo.

PR-003 resta parziale: **53PIDscalari, non tutti i PID standard**. Le strutture
standard composte/enum successive richiedono semantica/fonti e modello completo;
nessun passthrough arbitrario o servizio OEM/DTC/write introdotto per mascherare
questa mancanza. Restano manutenzione/prestazioni journal, sizing/retention,
risorse/latency fisiche, consumo e collaudo CloudKit dopo provisioning esterno.
Questa è una consegna software integrata e testabile, non una dichiarazione di
prodotto o di tutti i requisiti software completati.
