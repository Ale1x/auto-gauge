# Documentazione Auto Gauge

Iniziare dallo [stato corrente](project/STATUS.md) e dalle [istruzioni per le sessioni](../AGENTS.md). La documentazione è separata per argomento; i README nei moduli rimangono punti di ingresso vicini al codice.

| Sezione | Contenuto |
|---|---|
| [Progetto](project/README.md) | Stato, decisioni, tracciabilità e organizzazione del lavoro |
| [Prodotto](product/README.md) | Specifica di base e requisiti |
| [Architettura software](software/README.md) | Applicazione, discovery, manutenzione, UI gauge e consegna |
| [App iPhone](mobile/README.md) | Sviluppo locale, migrazione, configurazione Apple, recovery e OTA |
| [Firmware](firmware/README.md) | Build, endpoint, runtime, diagnostica e storage |
| [Protocolli](protocols/README.md) | Contratti app–display, framing, RPC, BLE, log, OTA e policy |
| [Hardware](hardware/README.md) | Gate K0, analisi elettriche/meccaniche e piani di collaudo |
| [Fornitori](suppliers/README.md) | Sourcing pannello, richieste e registro dei contatti |
| [Review software](reviews/software/README.md) | Revisioni tecniche e audit per incremento |
| [Verifiche versionate](verification/README.md) | Risultati V0.1–V0.13, con i limiti delle prove |
| [Verifiche UI](validation/README.md) | Screenshot, asset e report delle revisioni iOS |
| [Archivio](archive/README.md) | Handoff, brief e precedente README storico |

Le fonti originali rimangono in [evidence/](../evidence/SOURCES.md), i documenti hardware legati ai manifest accanto alle rispettive evidenze. Gli asset condivisi eseguibili restano in [shared/](../shared/README.md).

## Regole di manutenzione

- Una sola fonte corrente per lo stato: [project/STATUS.md](project/STATUS.md). Specifica e decisioni definiscono i requisiti; report e handoff mantengono il contesto temporale.
- Aggiungere documenti nella sezione pertinente e collegarli dall’indice di sezione. Evitare nuovi Markdown nella radice.
- Non modificare raw trace, PDF o manifest storici per uniformare i percorsi. La [mappa vecchio → nuovo](project/DOCUMENTATION_MAP.json) permette di seguire i riferimenti conservati nei vecchi artefatti.
- Aggiornare insieme script, generatori e link quando si sposta un file. Eseguire `python3 scripts/check_docs.py` e `python3 scripts/check_k0.py`.
