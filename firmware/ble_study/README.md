# NimBLE + endpoint C — studio di compilazione e binding

ESP-IDFv6.1/ESP32-S3, progetto separato dal firmware principale. **Main vuoto: nessuna radio, NVS, flash, task o periferica avviati dagli agenti. Non è un’immagine qualificata da installare.** Le funzioni pubbliche sono trattenute dal linker per verificare le vere dipendenze SDK. Chiamare in futuro `ag_nimble_bridge_start` o `ag_nimble_transport_start` avvierebbe realmente la radio: il main di questo studio non le chiama.

```sh
sh firmware/ble_study/build.sh
python3 firmware/ble_study/report_build.py
python3 scripts/generate_gatt_header.py --check
python3 -m unittest discover -s tests -p test_nimble_lifecycle.py -v
```

Build e size soltanto; lo script rifiuta argomenti e non inoltra comandi flash/monitor/eFuse. Usa la stessa cache SDK/toolchain descritta in `../DEVICE_BUILD.md`, commitfff9895c82d744c7237be8847347bdd1b07c6643. `build-report.json` registra opzioni reali, hash sorgenti/artefatti, simboli trattenuti e disassembly diapp_main senza chiamate. I binari e SDKCONFIG generati restano nella directory build ignorata.

## Catena collegata nel codice

`endpoint_bridge` associa il peer/generazione NimBLE a una lease `ag_connection`, applica `ag_admission`, poi collega RX→stream/RPC→`ag_device_endpoint`→configurazione/storici. Il codice C del core è quello usato nei test host e nel CLI, non una seconda implementazione della logica device. Il backend config/history resta iniettato e deve essere veritiero; il bridge non sceglie pin, partizioni, default o migrazioni.

- Nuovo telefono: il lookup SDK non trova un bond qualificato, quindi serve la finestra aperta da un evento locale. La passkey viene generata dal controller tramite entropia qualificata del chiamante, consegnata solo a callback display e SMP, poi azzerata. Nessun codice fisso o log della passkey.
- Telefono riconosciuto: record peer del vero store SDK, authenticated/SC/LTK/key16, senza nuova passkey. Identità provvisoria del link è legata a handle/generazione e successiva identità risolta; non viene fidata una MAC ricevuta inJSON.
- Ogni accesso privato e chunk di indicazione richiede stato corrente encrypted/MITM/bonded/SC/128bit oltre all’autorizzazione del controller. La sola connessione, CCCD ohello non conferisce autorizzazione.
- Write With Response soltanto; responseindicate senza notify. CCCD deve essere attivo prima di accettare chunk applicativi. Frame≤4112/chunk≤min(MTU−3,512), una indicazione in volo. StatusSDK0 significa invio, **solo BLE_HS_EDONE conferma** il chunk.
- Il bridge trattiene la progressione dell’endpoint mentre attende la conferma dell’intero frame. Per apply, received deve essere interamente confermato prima del successivo step di commit. Disconnect prima di quel punto non applica; dopo un commit già completato non si inventa rollback. Le emissioni usano nuovamente il clock dopo IO sincrono.
- Timer e callback host sono serializzati. Tag immutabile per connessione impedisce che un evento vecchio con handle riusato confermi/distrugga la connessione nuova. La generazione32bit non fa wrap; al limite occorre nuovo boot con nuova identità boot del core.
- Advertise solo se il callback locale qualifica stato awake; usa UUID servizio e indirizzoRPA inferito dal controller ESP32-S3. Il helper host-basedrpa_config è ESP32-only nel tag fissato e non viene usato. Pairing legacy/debugkeys e auto-deletion repeat-pairing sono disabilitati.

## Contratti del chiamante e guasti

Prima dell’inizializzazione del bridge: connessione con bootID fresco, journal reset ripristinato/clean, store inizializzato, backend storico bounded, entropia e display reali disponibili. Il chiamante inizializza NVS prima di avviare NimBLE. `CONFIG_BT_NIMBLE_NVS_PERSIST=y` è obbligatorio; il flusso SDK fissa i record di sicurezza prima dell’eventoENC_CHANGE. Questa integrazione non dimostra comunque durabilità fisica dei bond dopo power loss, né protezione delle chiavi a riposo: servono configurazione e prove dedicate.

Context e oggetti hanno durata statica/programmalifetime, anche su startup fallito. Nessuna rientranza o accesso da task concorrenti; eventi locali vanno serializzati sul proprietario. La callback display deve completare la visualizzazione locale prima di restituire successo. Il callback RNG deve fallire se la sorgente non è qualificata: `esp_random` senza una fonte d’entropia attiva non basta per deduzione dal solo nome API. Valgono i vincoli in `shared/BLE_ADMISSION_PROPOSAL.md`.

La creazione taskFreeRTOS viene controllata direttamente: il wrapper void della releaseSDK ignora il risultato dell’allocazione. OOM e startup parziale hanno cleanup e fault esplicito. La chiusura distingue richiesta di terminate riuscita da disconnessione effettiva: tre tentativi su errore, attesa bounded della conferma se richiesta accettata, slot pulito se il descriptor è realmente assente. Fallimento persistente, timer perso, hostreset/exit, indirizzo non disponibile o pressione store producono fault e bloccano operazioni private. **Nessun restart/repair automatico, cancellazione bond round-robin o eraseNVS.** Per questo studio i fault richiedono gestione esplicita del proprietario/reboot.

Otto bond e stack host32768byte sono limiti H di sviluppo, non Nmassimo di prodotto e non stack sufficiente misurato. La gestione della cache piena/riammissione locale resta da completare; non si usa factory reset per cambiare telefono. Lo stack endpoint/RPC/history annidato è significativo: la sola size del binario non lo qualifica. La perdita di awake chiude dati/link e ferma advertising; **non esiste ancora uno stop/power-management completo del controller**, quindi nessuna qualifica standby è dichiarata.

## Verifiche e limiti

Build reale SDK con tutti9entrypoint pubblici e core collegati. Harness host: sorgenti del trasporto/bridge immutati con shim SDK test-only e core C reale; guasti task/terminate/timer, eventi obsoleti, status0/EDONE, limiti e revoca, ammissione locale→hello→received→commit/readback, A–B–A e IO lento. [Review](../../reviews/NIMBLE_STUDY_REVIEW.md), [confine dei mock](tests/README.md).

Queste prove non eseguono ATT/SMP/FreeRTOS/controller reali, non dimostrano interoperabilità iPhone, latenza, sicurezza RF, consumo, durata della flash o disponibilità dell’interfaccia locale. App nativa restahello-only finché questo stack non viene qualificato. Il firmware principale conserva il proprio main inerte; questo progetto non lo sostituisce.
