# Ottavo incremento: runtime autonomo e percorso OTA

24 ottobre 2026. Requisiti v0.2.8 invariati, nessuna attivazione hardware/radio/cloud o membership Apple.

## App e coerenza diagnostica

APP-10 estende le formule storiche da7 a53PID scalari già implementati dal firmware. Quattro vettori per PID sono prodotti eseguendo il decoder C con ASan/UBSan e consumati readonly dai test Swift; freshness verificata in CI. Nomi/unità italiani, frazioni e U32 preservati, supporto effettivo del gauge distinto dalle formule disponibili. Nessuna nuova capability viene inventata nel simulatore. [Review del coordinatore](../reviews/APP10_REVIEW.md).

## Runtime firmware

FW-11 collega scheduler Service01, qualità delle misure e coda di registrazione al sink journal, senza dipendenza da telefono connesso. Configurazione e job devono concordare con capability realmente fornite dal chiamante. Il budget di registrazione è globale **≤1Hz perPID anche fra più ECU**; la misura live conserva comunque le provenienze. Nessun doppio record per tick invariati, soppressioni di rate distinte dalle perdite producer.

Coda16record bounded/drop-newest; append singola per drain. BUSY richiede nessuna mutazione; errore ambiguo può seguire un commit e ferma il runtime senza retry. SID fresco e avvio sessione esplicito, nessuna formattazione o riparazione automatica del supporto. Guardie CAN e FlowControl rimangono verificate prima del handoff e dopo revoche. I test non sono trasmissioni fisiche: un adapter TWAI deve ricontrollare policy/invio e completamento effettivi. I contatori producer sono RAM diagnostica, non gap persistenti dimostrati. [API e limiti](../firmware/RUNTIME.md), [review indipendente](../reviews/RUNTIME_REVIEW.md).

## OTA SDK isolato

OTA-03 implementa un driver che chiama le API SDK di begin/write/end sullo slot inattivo, controlla hash dei byte riletti e descriptor, quindi espone una select separata con nuovo hash. Guasti/revoche dopo select rimangono ambigui: nessun retry cieco, reboot, conferma salute o avanzamento del floor. La review ha corretto il caso SDK begin che pubblica un handle prima di fallire l’erase; abort e cleanup incerto sono ora espliciti. Un rifiuto prima dell’ammissione non occupa l’installer idle o associa il lease/clock del chiamante rifiutato.

Il **verificatore crittografico target del manifesto e il provisioning delle chiavi non sono implementati**: hook fidato obbligatorio, mai booleano remoto. Python host firma/preflight e driver C restano parti distinte. Nessun servizio update BLE/RPC disponibile. Configurazione separata4MiB factory+due OTA da1MiB con rollback; non è il memory map V1. Il main vuoto non invoca le7entry point trattenute nel link. Build SDK reale riuscita, immagine196688byte. Gli8gruppi dell’harness usano shim SDK e modello hash **non crittografico**, senza attribuirgli garanzie SHA/firma o test fisici. [Studio](../firmware/ota_study/README.md), [review sorgente SDK](../reviews/OTA_INSTALLER_SOURCE_REVIEW.md), [review/harness](../reviews/OTA_INSTALLER_REVIEW.md).

## Verifica finale e stato prodotto

`python3 scripts/check_software.py`: **85testPython,121Swift e16/16CTest superati**, ASan/UBSan per il C e build iOS Simulator unsigned riuscita. Dopo l’ultimo controllo di ammissione OTA sono stati ripetuti gli8gruppi sanitizzati interessati e la build/report SDK OTA: passati, altre sorgenti invariate. Build genericdevice unsigned riuscita nel filone app. Crossbuild principale ESP32-S3:18core+4condivise, main inerte; runtime compilato ma non avviato. Studio OTA separato con vere dipendenze esp_ota/PSA linkate e main senza chiamate. Progetto e risorse rigenerabili, registri/evidenze e diff controllati separatamente.

Le basi host dell’autonomia avanzano, ma integrazione e qualifica fisica restano aperte: TWAI/policy IGN reale, scheduler/flash/stack sotto carico, recovery READ_ONLY, persistenza efficiente in sync, interop BLE/iPhone, trasferimento OTA e salute/rollback, CloudKit live e QA. Il journal resta circa51minuti a5campioni/s su2MiB prima dei controlli di sessione; questo incremento non cambia il formato o la retention. Nessuna funzionalità veicolo o aggiornamento installato è dichiarato da queste build.
