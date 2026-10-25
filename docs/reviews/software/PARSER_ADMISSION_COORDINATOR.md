# Lettura del coordinatore — parser e ammissione

24 ottobre 2026. Lettura separata dell’implementazione; nessuna modifica ai sorgenti esaminati.

## Parser wire-config C

Esaminati `shared/c/ag_config_json.{h,c}`: candidato locale e copia finale soltanto su successo; cardinalità controllate prima degli accessi; integer overflow verificato prima della moltiplicazione; stringhe ASCII escaped decodificate prima della maschera duplicati; ogni container/schema chiuso ha chiavi e conteggi finiti. Il percorso massimo ordinario attraversa6container, non ricorre su un JSON arbitrario. Il limite512token è un contatore, non memoria allocata per512nodi.

Nessun nuovo accesso fuori limite o bypass concreto individuato. Il coordinatore ha eseguito l’intera suite Python:53test, inclusi8test parser con ASan/UBSan e10OTA, superati. La prova non è fuzzing esaustivo. Il sottoinsieme ASCII, rifiuto unknown e differenza dai riferimenti che normalizzano campi extra sono documentati in `docs/protocols/CONFIG_JSON_C.md`; non affermare accettazione di ogni JSON semanticamente esteso. Stack target statico e limiti in `docs/firmware/PARSER_BUILD.md`.

## Controller ammissione

Esaminati `firmware/include/ag_admission.h` e `src/admission.c`: bridge verificatore attivo soltanto nel percorso interno serializzato, hook sicurezza obbligatorio e ri-lettura prima degli accessi privati, passkey generata con rejection sampling bounded, lease/bootID e journal/manutenzione preservati. Nessun endpoint remoto confronta direttamente il codice o autorizza un client con il solo booleano connected.

Nessun nuovo difetto concreto individuato nei contratti letti. Review indipendente e suite dedicata: `docs/reviews/software/ADMISSION_REVIEW.md`. Rimangono obblighi del vero adapter: chiudere GAP, fermare indicazioni/queue, cancellare codice dal display e ricavare evidenza LESC/identità/bond corrente dal trustedstack. Non si dichiara autenticazione radio eseguita.

La fonte SDKv6.1 locale include `ble_store_value_sec.sc` e `authenticated` in `components/bt/host/nimble/nimble/nimble/host/include/host/ble_store.h`; `ble_sm_fill_store_value` valorizza tali campi in `host/src/ble_sm.c`. Il futuro binding può usare evidenza del bond correttamente risolto insieme a SC-only/stato corrente; non basta il descriptor GAP, che non esponeSC direttamente. Questa lettura non prova il binding completo né autorizza la stampa/estrazione delle chiavi.
