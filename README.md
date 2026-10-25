# Auto Gauge

Repository privato: [Ale1x/auto-gauge](https://github.com/Ale1x/auto-gauge).

Gauge OBD-II con display AMOLED, app iPhone in italiano e firmware ESP32-S3. Hardware e software sono in sviluppo: lo schema A0 contiene blocchi aperti; app, diagnostica e protocolli avanzano con simulazione e test host.

| Area | Punto di ingresso |
|---|---|
| Organizzazione e stato | [WORKSTREAMS.md](WORKSTREAMS.md) |
| Requisiti e decisioni | [SPEC_V0.2.md](SPEC_V0.2.md), [DECISIONS.md](DECISIONS.md) |
| App iOS | [mobile/README.md](mobile/README.md), `mobile/AutoGauge.xcodeproj` |
| Firmware | [firmware/README.md](firmware/README.md) |
| Contratto app–device | [shared/PROTOCOL_V0.1.md](shared/PROTOCOL_V0.1.md) |
| Schema KiCad | [hardware/README.md](hardware/README.md), `hardware/auto_gauge.kicad_pro` |
| Gate e blocker | [GATE_K0.md](GATE_K0.md) |

## Verifica software

Su un Mac con Xcode, Swift, CMake e compilatore C:

```sh
python3 scripts/check_software.py
```

Il comando verifica casi condivisi e test Python, confronta i codec C/Python con sanitizzatori, compila e testa il nucleo C diagnostico, esegue i test Swift e compila l'app per il simulatore iOS senza firma. Usa directory temporanee. `--skip-ios-build` omette soltanto la build dell'app e richiede comunque Swift per il package dei modelli.

Queste prove non comprendono esecuzione sul device, BLE reale, cloud live, consumo standby o test automotive. La build ESP-IDF ESP32-S3 è verificata separatamente in `firmware/DEVICE_BUILD.md`. App e firmware dichiarano le proprie parti simulate o non implementate nei rispettivi README. Non collegare al veicolo uno schema incompleto.

Risultati del secondo incremento: [shared/VERIFICATION_V0.2.md](shared/VERIFICATION_V0.2.md). Policy approvata sui client e reset separato: [shared/CONNECTION_POLICY_V0.1.md](shared/CONNECTION_POLICY_V0.1.md), secondo DEC-011.

Il terzo incremento integra storici locali nell'app, gestione logica di una connessione con più client sequenziali e studio KiCad separato del preregolatore USB. Verifiche: [shared/VERIFICATION_V0.3.md](shared/VERIFICATION_V0.3.md).

Il quarto incremento collega configurazione e storici allo stesso endpoint simulato, applica SwiftUI Pro, aggiunge paginazione interoperabile e persistenza configurazione con power-loss simulato. La build target ESP32-S3 riesce; resta uno skeleton inerte. Dettagli e limiti: [shared/VERIFICATION_V0.4.md](shared/VERIFICATION_V0.4.md).

Per i controlli portabili Python/C: `python3 scripts/check_host.py`. GitHub Actions li esegue su push/PR; la build mobile su runner macOS è avviabile manualmente. Configurazione Apple approvata e stato del provisioning: [shared/APPLE_CONFIGURATION.md](shared/APPLE_CONFIGURATION.md).

CloudKit: adapter nativo e upload idempotente predisposti, testati con database finto. Il team gratuito non abilita CloudKit; servono membership Developer e provisioning prima delle prove live. Download/restore sono ora implementati con test fakeDB; collaudo cloud live, BLE reale, OTA e qualificazione hardware restano aperti. Nessuna iscrizione o spesa attivata.

Gli incrementi successivi aggiungono recupero CloudKit paginato, CoreBluetooth di sviluppo limitato ahello, controller d’ammissione locale e codec C bounded. Il buffer persistente copy-on-write e la verifica OTA offline sono modelli testati, con limiti espliciti; non costituiscono backend flash o aggiornamento operativo della V1. Dettagli: [shared/VERIFICATION_V0.5.md](shared/VERIFICATION_V0.5.md).

L’endpointC ora collega autorizzazione, RPC, configurazione e storici; i test Swift dialogano con il suo processo binario e verificano anche fsync/readback. La diagnostica copre53PIDscalari. Uno studioSDK separato collega il serverNimBLE al medesimo endpoint senza avviare radio. Stato e prove: [shared/VERIFICATION_V0.6.md](shared/VERIFICATION_V0.6.md).

Il settimo incremento aggiunge grafici dagli archivi importati, journal storico append-only con adapter endpoint e preflight OTA sullo stesso snapshot firmato. Corrette altre due condizioni di guasto del server BLE. Verifiche e limiti, inclusa durata effettiva del nuovo formato: [shared/VERIFICATION_V0.7.md](shared/VERIFICATION_V0.7.md).

L’ottavo incremento collega polling e registrazione in un runtime autonomo host, estende le formule app a53PID con oracle C ed aggiunge uno studio SDK per scrittura/verifica dello slot OTA inattivo. Main ancora inerti e verificatore del manifesto target da integrare: [shared/VERIFICATION_V0.8.md](shared/VERIFICATION_V0.8.md).

Il nono incremento aggiunge verifica pacchetto OTA offline Swift e verifica manifesto C/PSA linkata nello studio SDK. Vettori comuni con firme reali e prova nativa TF-PSA-Crypto verificano l’interoperabilità; main inerte, trust di produzione e trasferimento restano da integrare. Esiti e limiti: [shared/VERIFICATION_V0.9.md](shared/VERIFICATION_V0.9.md).

Il decimo incremento implementa sender Swift e controller C per trasferimento OTA a blocchi con ACK esatti, stop prima della select e blocco dei retry dopo esiti incerti. Un adapter li collega al verificatore/installer nello studio SDK; trasporto BLE/main e chiavi di produzione restano da integrare. [shared/VERIFICATION_V0.10.md](shared/VERIFICATION_V0.10.md).

L’undicesimo incremento porta il trasferimento OTA nei frame AG con frammentazione, correlazione, timeout e chiusura senza retry. Le prove Swift→C attraversano il vero endpoint framed; lo studio SDK collega l’intera catena con main inerte. Nessuna route BLE/UI è attivata. [shared/VERIFICATION_V0.11.md](shared/VERIFICATION_V0.11.md).

V0.12 chiude i filoni software avviati: stato OTA durabile/NVS, adapter flash/TWAI, discovery, controller UI gauge, catalogo sessioni e preparazione OTA nell’app; sessioni native predisposte con qualifica locale negata per default. Restano composizione main/boardprofile e integrazioni esplicite, non soltanto prove hardware. [Verifica V0.12](shared/VERIFICATION_V0.12.md), [stato di completamento](docs/SOFTWARE_COMPLETION.md).
