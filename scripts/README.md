# Strumenti del progetto

Eseguire dalla radice del repository. Gli script ricavano la radice dal proprio percorso; i documenti generati sono scritti nelle rispettive sezioni di `docs/`.

| Comando | Scopo |
|---|---|
| `python3 scripts/check_docs.py` | Controlla link locali, mappa degli spostamenti e ordine della radice |
| `python3 scripts/check_k0.py` | Verifica registri, aritmetica e hash delle fonti K0 |
| `python3 scripts/check_host.py` | Contratti, test Python e core C con CTest |
| `python3 scripts/check_software.py` | Host, package Swift e build iOS unsigned |
| `python3 scripts/build_software_bundle.py` | Pacchetto riproducibile di build e relative evidenze; richiede SDK locali |
| `python3 mobile/scripts/generate_project.py` | Rigenera progetto Xcode, fixture e catalogo StoreKit locale |

Gli script `build_k0_registers.py`, `build_feasibility_models.py`, `build_power_envelope.py` e `build_buck5_screen.py` rigenerano documenti/registri: eseguirli soltanto per aggiornare esplicitamente quegli output e controllarne il diff. `build_mechanical_concepts.py` scrive il sorgente in `hardware/mechanical/`.

Le esportazioni KiCad e gli studi SDK hanno istruzioni nei rispettivi [documenti hardware](../docs/hardware/README.md) e [firmware](../docs/firmware/README.md). Le verifiche non attestano collaudi fisici e non autorizzano flash, ordini o attivazioni live.
