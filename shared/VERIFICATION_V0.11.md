# Undicesimo incremento: sessione OTA nei frame AG

24 ottobre2026. Requisiti v0.2.8/iOS17 invariati. Nessuna radio, flash, selezione boot reale, eFuse, trust di produzione, membership, servizio cloud o collegamento al veicolo.

## Catena implementata

APP-13 implementa `OTAPacketTransport` sopra `GaugePacketPipe`, `GaugeFrameCodec` e `GaugeStreamReassembler` esistenti. Usa una sessione esclusiva con gate host-development chiuso di default. `open()` osserva ready/CCCD e lega generazione/write-limit, senza scan/pairing. Request kind1/response kind2 con ID consecutivi e frammentazione a min(limite effettivo,512). Un solo scambio per volta; ACK16byte esatto nel frame correlato, nessun extra frame o trailing prefix. I codec framing/JSON RPC e i relativi golden non sono modificati.

Generazione/write-limit e token epoch sono controllati prima/dopo await, come deadline e timestamp ricevuti. Chiusura durante ready/write/receive impedisce successi tardivi. Errore/cancellazione/deadline o frame errato chiudono/disconnettono senza retry/reopen; una osservazione concorrente di generazione non interrompe il normale scambio. Il sender V0.10 conserva upload→READY e select separata, UNKNOWN dopo esiti incerti.

FW-14 implementa endpoint statico bounded con owner/trust/lease locali. Feed assembla e stage soltanto; step distinto chiama l’owner una volta e prepara ACK. Kind, lunghezza OTA, sequenza/replay, CRC e frame concatenati/trailing sono controllati prima del dispatch. Pending e ACK condividono deadline assoluta, con guardie di autorizzazione e clock monotono anche prima dell’emissione. Lease obsolete lasciano endpoint e owner byte-identici senza callback. Close/fault cancella una volta soltanto il proprio owner ancora attivo; owner selected/faulted/idle non viene mutato e il reconnect non resetta il vincolo un trasferimento/boot.

`framed_adapter` dello studio SDK compone endpoint→owner→payload adapter→verificatore PSA→installer. Init non esegue crypto/flash. La futura piattaforma deve scegliere localmente la route OTA esclusiva: il contenuto dei frame non commuta l’endpoint JSON e il bridge NimBLE non è modificato. [Contratto](OTA_FRAMED_V0.1.md), [review indipendente](../reviews/OTA_FRAMED_REVIEW.md).

## Prove eseguite

- **153testSwift** passati:143precedenti +8packet transport +2interop framedC. Mock coprono gate, limiti/cap512, ID/generazioni/write-limit, ACK corrotti/extra/partial, timestamp, scadenza dal primo byte, timeout/cancellazione, busy e close durante open/readiness/write/receive.
- Due prove processoC reale: sender→packet transport→pipe host con frammenti23byte→stream/endpoint/owner C, risposta ricomposta da chunk5byte. Upload termina READY senza select; select esplicita successiva. ACK SELECT generato dal C poi perso dal pipe test: senderUNKNOWN con possibile modifica boot, nessuna seconda select/reopen. Il CLI richiede `--trusted-fixture-maintenance` e usa backend **comportamentali**, senza RSA/hash/flash reali. Framing stdio u32LE del CLI è solo harness host, non ATT.
- Build iOSsim e genericdevice unsigned riuscite, senza provisioning. Nessuna UI o prova iPhone fisica aggiunta.
- `python3 scripts/check_host.py`: **100testPython e18/18CTest** passati. Sei gruppi endpointC e tre gruppi indipendenti con ASan/UBSan: stage senza side effects, replay, sole-frame/trailing/concat anche di BEGIN, timer frame/pending, revoca post-callback, leaseobsolete/reentry, cleanup singolo, reconnect senza reset e select incerta. Dopo l’ultimo controllo anticipato header, i gruppi mirati/review e crossbuildSDK sono stati ricontrollati.
- Harness aggregateSDK attraversa produzione framed endpoint/owner/adapter/installer e otto gruppi baseline installer. Manifesto, hash e API SDK sono doppi **non crittografici** espliciti. Verifica trust locale, ordine, stage prima di SELECT, close prima del dispatch, errore SELECT dopo cambio boot e revoca prima dell’emissione di ACK già preparato. Nessuna promessa di assenza di effetti dopo ACK perso.
- Crossbuild ESP-IDF fissato riuscito, **18entry point forzate** (7installer+2manifesto+3payloadadapter+6framedadapter) e dipendenze AG/controller reali; immagine**260608byte**, main vuoto. Link reale PSA e APIOTA SDK; nessun avvio su target. Report sorgenti/artefatti aggiornato. Il binario dello studio non dimensiona il prodotto V1.

Progetto Xcode/3JSON rigenerati identici, checkK0, diff whitespace, hash report/review e scansione file Git-eligible controllati prima della pubblicazione. Nessuna nuova chiave privata/credenziale/binarioSDK o fileoltre5MiB; resta il PDF storico già versionato. La prova nativecrypto V0.9 resta separata sui suoi sorgenti invariati e non diventa esecuzione radio/flash.

## Limiti da preservare

Trasporto cooperativo su cancellazione/deadline, pipe esclusivo e chiamate serializzate sono prerequisiti. Il target richiede callback affidabili, polling periodico e un solo proprietario di tutti i writer OTA/partizioni. take_output non dimostra invio/confirm: la piattaforma deve verificarli e chiudere la sessione su errore. Hello/CCCD/CRC/generazione non attestano autorizzazione OTA o sicurezza BLE.

Restano route nativa autenticata BLE/RPC/main e manutenzione concreta, trust/provisioning/rotazione/revoca, hardwareID/mappaV1, salute/rollback/floor atomico con fallback e recoveryUSB. Nessuna riconciliazione di select ambigua o resume viene inventata; nessun reboot automatico. Stack/heap, provider ESP32, tempi, brownout e interruzioni fisiche restano da qualificare. Anche runtime fisico/TWAI/journal, hardware/standby e CloudKit live rimangono aperti. Questo incremento non dichiara un aggiornamento installabile o prodotto completo.
