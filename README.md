# Auto Gauge

Gauge automotive con display circolare, app iPhone in italiano e firmware ESP32-S3. Il progetto comprende software, protocolli, studi hardware e relative evidenze. Il software è sviluppabile e verificabile su host; la qualifica del prodotto fisico è ancora aperta.

## Da dove iniziare

- **[Stato corrente](docs/project/STATUS.md)**: cosa funziona, limiti e prossimi lavori.
- **[Indice della documentazione](docs/README.md)**: specifiche, decisioni, architettura, hardware e verifiche.
- **[AGENTS.md](AGENTS.md)**: istruzioni operative per le prossime sessioni.

## Struttura

| Percorso | Contenuto |
|---|---|
| [mobile/](mobile/README.md) | App SwiftUI, package GaugeCore, test e progetto Xcode |
| [firmware/](firmware/README.md) | Core C, target ESP-IDF, adapter e test |
| [shared/](shared/README.md) | Schema, fixture, codec C e implementazioni di riferimento condivise |
| [hardware/](hardware/README.md) | KiCad, BOM, meccanica e studi dei circuiti |
| [docs/](docs/README.md) | Documentazione organizzata per argomento e archivio storico |
| [evidence/](evidence/SOURCES.md) | Fonti originali, datasheet, manifest e modelli |
| [scripts/](scripts/README.md) | Generazione, analisi e verifiche riproducibili |
| [tests/](tests/README.md) | Test Python/C di integrazione e template per misure |
| `output/` | Artefatti locali generati, esclusi dal versionamento dove previsto |

Gli originali locali del fornitore in `email-response/` restano fuori dal versionamento e dai pacchetti software.

## Verifiche

Dalla radice del repository:

```sh
python3 scripts/check_docs.py
python3 scripts/check_k0.py
python3 scripts/check_host.py
```

Su Mac con Xcode e Swift, per una verifica software completa:

```sh
python3 scripts/check_software.py
```

Per lavorare soltanto sull’app: [istruzioni mobile](mobile/README.md) e [sviluppo locale iOS](docs/mobile/DEVELOPMENT.md). Per firmware e pacchetti riproducibili: [consegna software](docs/software/SOFTWARE_DELIVERY.md).

Gli esiti dei test host o delle build non equivalgono a collaudo automotive, radio, CloudKit live o qualifica hardware. Le verifiche storiche restano nell’[archivio delle verifiche](docs/verification/README.md).
