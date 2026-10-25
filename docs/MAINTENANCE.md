# Provisioning e manutenzione persistente

25 ottobre 2026. Implementazione software e modelli di fault, nessuna operazione
su un dispositivo. Il profilo hardware/USB/pulsanti e la cancellazione concreta
dei bond restano responsabilità di un adapter locale qualificato.

## Configurazione iniziale

`ag_config_store_open` continua a leggere soltanto: una partizione vuota non
diventa automaticamente una configurazione di default. L'API separata
`ag_config_store_provision_blank` accetta una configurazione valida di revisione1
soltanto se **entrambi** gli slot da4096byte sono completamente cancellati.
Scrive record, verifica il contenuto non committato, scrive marker e rilegge il
record completo; non cancella o sovrascrive alcun settore. Un singolo byte sporco
anche nell'area riservata o nello slot inattivo impedisce il provisioning.

Una chiamata che ha iniziato l'IO non si ripete nello stesso owner. Dopo un
errore si riapre/riconcilia: un record valido può essere già presente, un record
parziale richiede manutenzione esplicita. Il chiamante seleziona localmente la
configurazione e garantisce esclusione con RPC/runtime/OTA e alimentazione;
l'API non è esposta come comando remoto. Il profilo banco usa memoria volatile
dichiarata, non rende questi dati un default prodotto.

## Journal del reset su NVS

`firmware/platform_study/main/maintenance_store` usa il core esistente
`ag_factory_reset_*` e un record canonico32byte nella namespace `ag_reset_dev`,
chiave `state_v1`. Serializza magic/versione/generazione/fase/mask/checksum senza
padding della struct, valida i byte riservati e il CRC del core. CRC rileva
corruzione accidentale, non autentica il contenuto.

Il bind apre prima READONLY e valida un record esistente; solo dopo apre RW e
restituisce snapshot e callback. Non crea namespace, non inizializza/cancella NVS,
non fabbrica un reset CLEAN se il record manca o è corrotto. La creazione separata
`ag_maintenance_provision_reset` richiede evento fisico locale qualificato,
alimentazione USB valida e conferma locale; crea soltanto un record assente.
Una namespace senza record non implica configurazione/log/bond cancellati: CLEAN
significa soltanto che non esiste un reset in corso.

Ogni aggiornamento NVS richiede set, commit e readback identico. Un errore può
seguire effetti durabili (anche il set); l'adapter blocca ulteriori mutazioni.
Nessun tentativo cieco di annullare o ripetere una scrittura. Al nuovo boot si
rilegge la fase effettiva; il core richiede di nuovo arm/conferma fisici prima
di riprendere un reset interrotto. Questo percorso non autorizza un reset BLE.

Sequenza: ARMED → INTENT → credenziali cancellate → configurazione cancellata →
log cancellati → CLEAN. INTENT è durabile prima delle cancellazioni. I callback
config/log usano le partizioni già validate da `partition_io`, con cancellazione
e readback per settore. Un errore blocca il resto; dopo un nuovo boot e una nuova
conferma può ripetersi la fase idempotente non ancora registrata. Il chiamante
deve fermare completamente BLE/CAN/runtime/OTA prima delle fasi e garantire le
condizioni di alimentazione per l'intera operazione sincrona, non soltanto per
la lettura iniziale di un pulsante. Il tempo fisico del reset non è misurato.

Il callback credenziali è obbligatorio e viene fornito dal port della piattaforma:
deve eliminare bond persistenti e cache runtime, **preservando** namespace reset,
trust e release floor. Non è implementato mediante `nvs_flash_erase` e non esiste
qui una cancellazione globale del chip. Il bind non finge che il callback sia
una prova dello stack NimBLE o delle periferiche.

## Verifiche

- `gauge_config_store_tests`: nove gruppi, inclusi provisioning su blank, rifiuto
  dei byte sporchi, fault in ogni byte delle scritture e letture interrotte,
  riapertura senza overwrite; restano le24653frontiere di fault del commit.
- `python3 -m unittest discover -s tests -p test_maintenance_store.py -v`:
  produzione C con NVS/SDK comportamentali e ASan/UBSan; quattro gruppi coprono
  probe/provisioning, corruzione di ogni byte, riavvio dopo ogni fase, errore dopo
  effetto NVS, readback errato, latch, erase parziale e geometria invalida.
- Il progetto SDK platform collega le API NVS effettive e l'adapter; il suo main
  rimane vuoto e non esegue provisioning/reset. Le prove non qualificano brownout,
  wear, durata flash o temporizzazione fisica.
