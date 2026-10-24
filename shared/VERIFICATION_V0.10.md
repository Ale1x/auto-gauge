# Decimo incremento: controller e sender OTA bounded

24 ottobre2026. Requisiti v0.2.8/iOS17 invariati. Nessun dispositivo, veicolo, provisioning, chiave prodotto, download, flash, selezione boot reale, eFuse, costo o servizio cloud attivato.

## Comportamento implementato

APP-12 trasferisce soltanto `VerifiedOTACandidate` immutabili tramite un’interfaccia di sessione a payload completi. L’actor sender mantiene una singola operazione fra await, verifica generazione prima/dopo ogni scambio e ACK esatti per ID, offset manifesto/immagine, stato e flag. Gate `allowHostDevelopmentMutations` chiuso di default. Upload termina READY; `select()` è separata, esplicita e non riavvia il gauge. Timeout, cancellazione, ACK perso/malformato/rifiutato o cambio generazione dopo il tentativo chiudono il sender UNKNOWN. Nessun retry, resume, restart, cancel automatico o select successiva incerta.

FW-13 implementa payload binari bounded, owner con manifesto4096byte e blocchi≤1024byte. Offset/ordine/ID errati non scrivono o avanzano progressi; una lease diversa non può toccare l’owner corrente. Autorizzazione locale e clock monotono pre/post callback, deadline totale≤15min e idle, poll obbligatorio anche senza traffico. Un trasferimento per boot. Prima di begin si assembla tutto il manifesto; callback fidata deve verificarlo e iniziare l’installer. Errore dopo callback è terminale e può avere effetti già avvenuti: cleanup una volta, nessun retry. Select mantiene un flag conservativo prima della callback e non tenta cleanup/retry della selezione.

`transfer_adapter` collega questo owner al vero `ag_manifest_verify` e `ag_ota_installer_*`, con copie locali di trust/platform config e nessuna chiave predefinita. Il floor rimane invariato; nessuna conferma salute o reboot. Il mapping della normale cancellazione installer (DENIED+FAULT, handle inattivo) è stato corretto durante review: errore abort continua a essere incerto. Un errore del manifesto impedisce SDK begin. [Contratto](OTA_TRANSFER_V0.1.md), [review](../reviews/OTA_TRANSFER_REVIEW.md).

## Prove eseguite

- **143testSwift**, tutti passati:130precedenti +11codec/sender +2interop col processoC. Fixture comuni41request+20response, roundtrip positivi e rifiuti stretti. Candidate vere prodotte con firme OpenSSL effimere e verificate da Security/CryptoKit; nessun costruttore verificato finto aggiunto.
- Interop Swift→CLI C persistente: upload fino READY senza select; select successiva separata. ACK immagine accettato dal controller C poi scartato dal transport test: sender UNKNOWN, nessun secondo chunk/restart/select. Questo CLI usa callback **comportamentali**, non verifica manifesto/hash/flash e richiede ammissione fixture esplicita. Scambio payload stdio: il framing AG/correlazione/reassembly resta responsabilità del futuro transport.
- Build iOSsim e genericdevice unsigned riuscite, senza membership o provisioning. Nessun test su iPhone fisico o UI nuova.
- `python3 scripts/check_host.py`: **98testPython e17/17CTest** passati. Sei gruppi ownerC, tre gruppi indipendenti review,10.000payload malformati senza mutazioni/callback; revoca, deadline totale anche con idle fresco, reentry auth/backend/cancel, offset/routing, cleanup singolo e select ambigua sotto ASan/UBSan.
- Harness adapter esegue controller+adapter+installer di produzione e gli8gruppi baseline installer, con doppi SDK/hash/manifesto esplicitamente **non crittografici**. Verifica binding trust locale, nessuna write prima della verifica, cancel pulito/abort fallito, manifest rejection e select che cambia boot prima di fallire. Non si attribuisce crittografia reale a questo harness.
- Studio ESP-IDF reale ricompilato:12entry point forzate (7installer+2manifesto+3adapter) e dipendenze owner, immagine**256912byte**, `app_main` vuoto. VerificatorePSA e APIOTA SDK vere linkate, mai invocate su dispositivo. Warning Xtensa di indentazione corretto e build ripetuta. Report `firmware/ota_study/build-report.json` aggiornato con sorgenti adapter/owner e hash verificati.

La prova nativecrypto V0.9 rimane distinta e valida per i suoi sorgenti invariati; nessuna funzione crittografica è stata modificata. Non viene reinterpretata come prova end-to-end SDK/flash o provider ESP32.

## Integrità e limiti

Progetto Xcode/3JSON rigenerati identici, checkK0 e diff whitespace controllati prima del commit. Scansione file Git-eligible senza nuove credenziali/private key/binari/oversize; il PDF storico già versionato oltre5MiB resta distinto dai nuovi file. Report sorgenti e review ricontrollati.

Il timeout Swift richiede un transport che rispetti cancellazione/deadline: un’implementazione non cooperativa non eredita questa garanzia. Controller/adapter richiedono un proprietario serializzato esclusivo di tutti i writer OTA/partizioni, callback locali affidabili e polling periodico. Questi contratti non sono provati da hello o dal protocollo stesso.

Restano route autenticata BLE/RPC/main, policy di manutenzione concreta, trust/provisioning/rotazione/revoca, hardwareID e mappa V1, floor durabile atomico con fallback, boot health/rollback e recovery USB, sizing stack/heap e fault fisici. Nessun reset owner su reconnect, stato reconciliation o resume è implementato. Runtime fisico/TWAI/flash/log, hardware/standby e CloudKit live restano aperti. Questo incremento non è un aggiornamento OTA operativo o una dichiarazione di prodotto finito.
