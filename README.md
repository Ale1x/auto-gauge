# Auto Gauge

Gauge OBD-II con display AMOLED, app iPhone in italiano e firmware ESP32-S3. Hardware e software sono in sviluppo: lo schema A0 contiene blocchi aperti e il primo incremento software usa una simulazione e un nucleo testabile su host.

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

Il comando verifica i casi condivisi, compila e testa il nucleo C con sanitizzatori, esegue i test Swift e compila l'app per il simulatore iOS senza firma. Usa directory temporanee. `--skip-ios-build` omette soltanto la build dell'app e richiede comunque Swift per il package dei modelli.

Queste prove non comprendono ESP-IDF sul device, BLE reale, cloud, consumo standby o test automotive. App e firmware dichiarano le proprie parti simulate o non implementate nei rispettivi README. Non collegare al veicolo uno schema incompleto.
