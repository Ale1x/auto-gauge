# Audit reset credenziali NimBLE — V0.13

Review del sorgente locale SDK fissato a `fff9895c82d744c7237be8847347bdd1b07c6643`,
ESP-IDFv6.1, ESP32-S3. Solo lettura SDK e test comportamentali del nostro adapter:
nessun avvio radio, cancellazione di bond reali o NVS fisica.

## Esito

Non è corretto fornire un callback erase_credentials che restituisca successo
usando soltanto `ble_gap_unpair` o `ble_store_clear`. Il reset applicativo conserva
quindi il requisito di un eraser board qualificato per dati persistenti e cache
operative; se manca il callback, il profilo nativo resta failclosed. Non usare
`nvs_flash_erase`, erase dell'intera partizione NVS o cancellazione dei namespace
reset/boot/trust/floor per sostituire il lavoro mancante.

## Evidenze SDK lette

- `ble_gap.c:ble_gap_unpair`: richiede host abilitato e assenza di advertising/
  discovery. In diversi rami registra errori di rimozione IRK/controller o store
  senza propagarli nel risultato finale (`err` può restare0). Il suo successo
  non prova che tutte le liste/cache e credenziali siano state rimosse.
- `ble_store.c:ble_store_clear`: cancella i tipi OUR_SEC, PEER_SEC, CCCD, CSFC,
  PEER_ADDR, LOCAL_IRK (ed EAD se configurato) attraverso store_delete_cb. Non è
  uno stop radio/controller e non basta a qualificare resolving list e stato
  autenticato del link.
- `ble_store_config.c`: cancellare un elemento decrementa il conteggio e può
  fare memmove; poi persiste il tipo. Errori possono seguire cambiamenti RAM.
  Il tail dell'array non è una garanzia di azzeramento forense delle vecchie
  chiavi. Non sostituire la distinzione fra cache non usabile e memoria zero.
- `ble_store_nvs.c`: persistenza confinata al namespace `nimble_bond` tramite
  nvs_erase_key/commit. Anche qui un errore può seguire effetti: non è permesso
  riprovare ciecamente o considerare completata la fase reset senza verifica.
- `ble_hs_pvcy.c`: la gestione IRK include la resolving list del controller per
  il profilo S3. Gli helper che la ripuliscono non costituiscono un unico API
  pubblico di purge completo/durabile dei bond da usare senza lifecycle.
- `nimble_port.c:nimble_port_stop`: attende semafori e un evento servito dal task
  host. Chiamarlo dentro owner_tick/task host bloccherebbe il task necessario a
  completare lo stop. Non è stato introdotto questo deadlock nella manutenzione.

## Componente implementato: quiesce locale

`ag_nimble_transport_quiesce()` viene invocato dal proprietario sul task host,
prima di entrare nei core applicativi (nessuna rientranza). Imposta un gate
permanente che nega nuovi collegamenti applicativi e dati privati, impedisce
advertising, richiede stop advertising e teardown bounded del link corrente.
La richiesta di terminate accettata non conta come disconnessione. True richiede
assenza di link fisico tracciato, advertising SDK inattivo e assenza di fault;
false può essere IN_PROGRESS oppure fault. Ogni CONNECT tardivo resta tracciato
per chiusura e non riapre ammissione o SMP.

La radio/controller non è deinizializzata da questa API. Non è una prova di
standby100µA e non dichiara erasure delle credenziali. I tick host continuano per
consentire manutenzione serializzata. Non esiste restart nel medesimo owner.

Per completare il callback credenziali occorre un lifecycle qualificato che
escluda tutti i link/GAP, pulisca e verifichi lo store persistente esatto e le
cache operative host/controller, preservi reset/trust/floor, propaghi qualunque
errore ambiguo e permetta recovery dopo riavvio. Non è implementato un successo
fittizio per chiudere questa dipendenza.

## Test nostro adapter

Il tredicesimo gruppo lifecycle usa i sorgenti produzione invariati con shim
SDK esplicito, ASan/UBSan: stop advertising, richiesta di teardown non ancora
confermata, callback di disconnessione, rifiuto dati/nuovo CONNECT e stop fallito
che lascia advertising attivo. Non esegue l'API reale di erasure né dimostra
comportamento RF/flash. L'undicesimo gruppo bridge verifica tick owner idle,
inizializzazione differita RPC e latch dopo owner fault.

## Hash dei sorgenti SDK consultati

- `0ad365b2a423b54e9ee4365cdb52a0ffefe4d70be144e36288cc50781f85fe5b` — `components/bt/host/nimble/nimble/nimble/host/src/ble_gap.c`
- `901b3671751f9d381c717adf4ba213d9a9b87a6162d33195f7cbd6fbd724a6ca` — `components/bt/host/nimble/nimble/nimble/host/src/ble_store.c`
- `bf5c4650e974e51a8055cf2b6ca278a81f9c47ccb9606445de28d6a43ae46756` — `components/bt/host/nimble/nimble/nimble/host/src/ble_hs_pvcy.c`
- `88257faee279574ed4651bdb95e73c4d16ed6137477af21b4c3cab736c0f24d7` — `components/bt/host/nimble/nimble/nimble/host/store/config/src/ble_store_config.c`
- `6e0751b414d760caeb9eab0eb109fc706000efa87baf7c25e4b5593e87528aba` — `components/bt/host/nimble/nimble/nimble/host/store/config/src/ble_store_nvs.c`
- `209d768fb6c6c184363863a17a2e94b66fc9d2e6ff91d55ddca14100b5e20677` — `components/bt/host/nimble/nimble/porting/nimble/src/nimble_port.c`
