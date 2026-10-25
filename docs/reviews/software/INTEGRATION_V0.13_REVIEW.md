# Review di integrazione V0.13

25 ottobre 2026. Review indipendenti tramite agenti della sessione, seguite da correzioni del coordinatore. Nessuna review hardware/criptografica o accesso a dispositivo è implicita.

## Composizione nativa — review indipendente app_recovery

Ambito: `native_profile` e contratti owner/TWAI/bridge/reset/renderer. Tre difetti riprodotti tramite lettura del codice e successivamente corretti:

1. Il renderer manteneva uno snapshot per circa3secondi (16righe/tick100ms), continuando a emettere un valore vecchio dopo cambio qualità/dashboard. Il profilo nativo completa ora ogni proiezione nel medesimo tick, con budget temporale esplicito `maximum_frame_ms`, controllo fra chunk e fault senza replay se oltre budget. La primitiva renderer resta incrementale e bounded; latenza reale del bus/host richiede qualifica del profilo. Nessun buffer-frame completo allocato.
2. RESET_ARM entrava in esclusiva e bloccava la radio prima di verificare USB. Ora USB è verificata prima delle transizioni; rifiuti/errori dell'arm sono gestiti e non si dichiara una fase durabile mai scritta.
3. Il SID era generato con `esp_fill_random` prima dello stack radio, senza prova delle condizioni di entropia. Ora usa quattro chiamate fallibili al provider locale di entropia richiesto dal profilo, con rifiuto dell'ID tuttozero e nessun fallback.

La review successiva comprende recovery del reset interrotto prima della radio, cleanup startup, timeout request/FC e i tre fix. Nessun ulteriore difetto concreto rilevato nell'ambito. `test_native_profile.py` compila produzione native+bridge e intero core C separatamente, con soli adapter SDK comportamentali:12scenari ASan/UBSan passati. Il ramo senza profilo rifiuta prima di prepare/IO/radio; gli scenari includono fail prepare/entropy, boot corrotto, recovery con nuova arm/conferma, credentialsfail senza retry, radioquiescefalse, request TX pendente, render timeout e composizione normale.

## Provisioning/reset — review indipendente ble_router

Ambito: config-store blank provisioning, adapter maintenance NVS e contratti di erase. Nessun difetto concreto rilevato. Verificati: scan completo8192byte prima di program, no erase/overwrite, one-attempt latch, readback; record32byte canonico, probeREADONLY prima di RW, no namespace/record CLEAN impliciti; intent durabile prima dei side effect; resume idempotente dopo nuova conferma; cancellazioni limitate alle partizioni validate.

Limite materiale confermato: il callback credenziali è ancora una dipendenza locale qualificata. Il test del reset non dimostra erasure del vero store+cache controller. L'audit SDK in `docs/reviews/software/BLE_CREDENTIAL_RESET_AUDIT.md` spiega perché `ble_gap_unpair`/`ble_store_clear` da soli non consentono un successo affidabile e perché lo stop bloccante sul task host causerebbe deadlock. Nessun eraser fittizio è stato collegato.

Dopo la review il coordinatore ha ampliato il test manutenzione con errore dopo effetto su ARMED/INTENT e interruzione a metà cancellazione log. I quattro gruppi manutenzione restano verdi con sanitizzatori. Le nove prove config includono provisioning e le24653frontiere byte del commit precedente.

## Correzioni del coordinatore sull'owner

Il coordinatore ha richiesto e verificato tramite regressioni: quiesce e scena indisponibile quando una config diventa incompatibile, fault che arresta e tenta cleanup una volta, mancata ripresa dopo esito ambiguo di commit, verifica health completa senza OR di bit inventati, clock fresco dopo reconcile boot. L'owner salta il timeout risposta soltanto mentre la richiesta Service01 è ancora in coda TX; FC conserva la deadline originale. Questi sono controlli del coordinatore, distinti dalle review indipendenti sopra.

## Snapshot SHA-256

- `f4d39f0d2c68d2318b0e2d51894a113407345445a18019865b404fd132a2bfac` — `firmware/idf/main/native_profile.c`
- `89e278b88696d1e8f7d93185eef101fc7fe2ff513bd78c3980c12f6c41d9dcb6` — `firmware/idf/main/native_profile.h`
- `e2658eaf2466ab862c52c7f437fa752fc8be53071bb6d05cbef40ea18dd22558` — `firmware/platform_study/main/maintenance_store.c`
- `f0b6384ca45b28e706c9bd3ebad12679eee41bc4b3e7cce90ac54c9bfe3c6ac1` — `firmware/platform_study/main/maintenance_store.h`
- `4c4bb5e0c112b8fd4ce3e76a1fb78df5fbe975e22ca25f3f6b41418ffb20ecfe` — `firmware/src/config_store.c`
- `dd1515f16bf5434d7a48a19fa899ac5de1c2e46195c013d5d7ec9d53436bf823` — `firmware/include/ag_config_store.h`
- `4c1f991023b694356137c563baf4518ec668651875133d8adf18ed717e4a2a18` — `firmware/idf/tests/native_profile_driver.c`
- `90b901f84451a06af8e4f9ad0627019bd680a89c1bb305fb476ca446e587703b` — `tests/test_native_profile.py`
- `0195013b82624f12aa9cd45e4a43ffb032cad9cd6d7ddb1f87da3cc2ee51974e` — `firmware/platform_study/tests/maintenance_driver.c`
