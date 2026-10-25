# Build reale ESP32-S3 — SDK ufficiale v6.1

**Esito del 24/10/2026: build e size riusciti; nessuna esecuzione su dispositivo.** Il progetto `firmware/idf` è stato cross-compilato e collegato per ESP32-S3, con bootloader e tabella partizioni. Il report riproducibile è `device-build-report.json`; binari, SDK, toolchain, venv e log di installazione non sono aggiunti a Git.

| Elemento verificato | Versione / risultato |
|---|---|
| Host | macOS, ARM64 |
| ESP-IDF ufficiale | tag `v6.1`, commit `fff9895c82d744c7237be8847347bdd1b07c6643` |
| Toolchain | `xtensa-esp-elf-gcc`, crosstool-NG `esp-15.2.0_20251204`, GCC 15.2.0 |
| Python nel venv isolato | 3.14.9, interprete Homebrew già disponibile |
| App `.bin` | 170624 byte |
| Immagine riportata da `esp-idf-size` | 170512 byte, prima del padding `.bin` |
| Bootloader `.bin` | 21120 byte |
| Tabella partizioni `.bin` | 3072 byte |
| Sorgenti core cross-compilate | 18, elencate nel report, inclusi journal storico, adapter endpoint e runtime |
| Sorgenti C condivise cross-compilate | `ag_config_json.c`, `ag_frame.c`, `ag_stream.c`, `ag_rpc.c`, elencate separatamente |

## Cache e installazione mirata

SDK: `~/.cache/auto-gauge/esp-idf-v6.1`. Strumenti/venv: `~/.cache/auto-gauge/idf-tools-v6.1`. Cache pip: `~/.cache/auto-gauge/pip`. Installato soltanto il compilatore `xtensa-esp-elf` richiesto da questa build; non sono installati debugger, OpenOCD, QEMU, compilatori RISC-V/ULP o strumenti GUI. CMake e Ninja esistenti sono riutilizzati. Il sorgente SDK include i suoi submodule ufficiali; questo non attiva funzioni radio o altri target.

La release ufficiale conferma il tag e ammette il metodo legacy per l'installazione. I comandi usati, con cache separata e nessuna modifica di shell/configurazione globale, sono equivalenti a:

```sh
mkdir -p "$HOME/.cache/auto-gauge"
git clone --depth 1 --branch v6.1 --single-branch --recursive --shallow-submodules --jobs 4 \
  https://github.com/espressif/esp-idf.git "$HOME/.cache/auto-gauge/esp-idf-v6.1"

IDF_TOOLS_PATH="$HOME/.cache/auto-gauge/idf-tools-v6.1" \
  /opt/homebrew/bin/python3.14 "$HOME/.cache/auto-gauge/esp-idf-v6.1/tools/idf_tools.py" \
  install --targets esp32s3 xtensa-esp-elf

IDF_TOOLS_PATH="$HOME/.cache/auto-gauge/idf-tools-v6.1" \
PIP_CACHE_DIR="$HOME/.cache/auto-gauge/pip" PIP_DEFAULT_TIMEOUT=30 \
  /opt/homebrew/bin/python3.14 "$HOME/.cache/auto-gauge/esp-idf-v6.1/tools/idf_tools.py" \
  install-python-env --features core
```

Prima del download è stato verificato il commit del tag tramite `git ls-remote`; l'installer ufficiale verifica i checksum degli archivi descritti nei propri metadati. Nessun `sudo`, modifica del profilo utente o installazione globale pip effettuati. I comandi sopra non vanno ripetuti se le stesse directory esistono già; non sovrascrivere un SDK diverso.

## Comando di build

Dalla radice del repository:

```sh
sh firmware/tools/build_esp32s3.sh
```

Lo script verifica il commit SDK, espone le variabili solo al processo figlio, imposta il target esp32s3 e il percorso `sdkconfig` dentro la directory di build ignorata, poi esegue esclusivamente `build size`. Non accetta azioni aggiuntive e non invoca flash, monitor o utility eFuse. Con l'installazione minima evita `export.sh`, che richiederebbe anche tool di debug non necessari; imposta esplicitamente `ESP_IDF_VERSION=6.1`, necessario all'estensione Python del component manager.

Sono disponibili override `AG_IDF_ROOT`, `AG_IDF_TOOLS`, `AG_IDF_PYTHON_ENV`, `AG_XTENSA_BIN` e `AG_IDF_BUILD_DIR` per riutilizzare una cache equivalente. Il commit SDK deve restare quello verificato. Il percorso Python di default è quello dell'installazione macOS 3.14; su un altro host impostare l'override della venv o usare l'ambiente ESP-IDF ufficiale corrispondente nella CI.

Per rigenerare il report dalla build esistente:

```sh
"$HOME/.cache/auto-gauge/idf-tools-v6.1/python_env/idf6.1_py3.14_env/bin/python" \
  firmware/tools/report_device_build.py \
  --sdk "$HOME/.cache/auto-gauge/esp-idf-v6.1" \
  --toolchain-bin "$HOME/.cache/auto-gauge/idf-tools-v6.1/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin" \
  --idf-python "$HOME/.cache/auto-gauge/idf-tools-v6.1/python_env/idf6.1_py3.14_env/bin/python" \
  --build firmware/idf/build --output firmware/device-build-report.json
```

Il report contiene versioni, hash SHA-256 dei binari, configurazione effettiva, layout di memoria, sorgenti core compilate e simboli `ag_` effettivamente rimasti nel link. I log completi locali sono in `~/.cache/auto-gauge/device-build.log` e `python-env-install.log`; i log SDK includono suggerimenti automatici di flash che **non sono stati eseguiti**.

## Interpretazione e limiti

Questa è una build dello **skeleton inerte**, non un firmware operativo sul gauge. Le quindici unità C superano la compilazione Xtensa, ma il linker elimina le funzioni non usate da `app_main`: nel binario restano soltanto `ag_can_policy_init`, `ag_can_tx_allowed`, `ag_hal_read_power` e `ag_session_clock_init` fra i simboli core. Le dimensioni quindi non comprendono l'uso reale di configurazione, decoder, ISO-TP, storico, connessione/reset e relativi buffer; mancano inoltre driver AMOLED/touch, asset, BLE autenticato, TWAI, flash log e OTA.

La configurazione di sviluppo resta quella SDK **2 MB, singola app factory, PSRAM disabilitata**, senza slot OTA o partizione log. Non è la mappa definitiva del candidato WROOM-N16R8 da 16 MiB/8 MiB, non dimostra che tutti i componenti finali entrino e non congela il sizing OTA da cui dipendono i 2 MiB approvati per i log. Secure boot/flash encryption non sono provisionati; l'immagine non è firmata o rilasciata come recovery di produzione.

Bluetooth è disabilitato nella configurazione. `CONFIG_ESP_WIFI_ENABLED=y` è un simbolo interno senza prompt del relativo SDK, derivato dalla capability del target; non equivale a inizializzazione Wi-Fi. L'ELF non contiene `esp_wifi_init`, `esp_bt_controller_init` o `nimble_port_init`, e `app_main` non chiama driver radio, display, CAN, scrittura flash o provisioning. Nessuna radio o periferica è stata attivata perché non è stato flashato né eseguito alcun firmware.

La build SDK emette note Kconfig/deprecazioni interne non bloccanti; non sono stati modificati i sorgenti vendor per nasconderle. Non è stato necessario correggere il C del core per ottenere il link. Non sono stati eseguiti test hardware, misure di standby, prove di boot sul modulo reale o conferme di compatibilità della vettura.

Fonti ufficiali consultate il 24/10/2026: [release ESP-IDF v6.1](https://github.com/espressif/esp-idf/releases/tag/v6.1), [guida ESP32-S3 stable v6.1](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/index.html), [installazione legacy Linux/macOS](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/get-started/linux-macos-setup-legacy.html).

## Studio BLE separato

`ble_study/` compila e collega adapterNimBLE e bridge al vero endpointC, conservando un proprio main vuoto. Bluetooth è abilitato soltanto nella configurazione di questo studio: non modifica il main/config del progetto sopra. I suoi entrypoint sono trattenuti nel link per verificarne le dipendenze, senza essere chiamati. [Procedura e limiti](../../firmware/ble_study/README.md); il report separato non qualifica l’immagine finale o il consumo.
