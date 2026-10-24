# Parser C condiviso: compilazione e stack

24 ottobre 2026. `shared/c/ag_config_json.c` è incluso nella libreria host `gauge_core` e nel componente IDF `main`; entrambi espongono `shared/c` come include directory. Il parser non è chiamato da `app_main` e viene eliminato dal link dello skeleton inerte. Le otto suite CTest precedenti restano verdi con ASan/UBSan; la suite funzionale del parser resta quella condivisa `tests/test_c_config_json.py`, separata da questi CTest. Nessun sorgente parser modificato.

## Dimensioni osservate

| Oggetto | Host ARM64 | ABI Xtensa ESP32-S3 |
|---|---:|---:|
| `parser_t` privato | 48 byte | 28 byte |
| `ag_config_t` candidato | 440 byte | 416 byte |
| Buffer stringa | 32 byte | 32 byte |

**512 token è un contatore massimo, non un array di512nodi.** Non sono allocati buffer proporzionali al limite token. La profondità massima8 è anch’essa una guardia; il percorso schema ordinario più profondo attraversa root, array dashboard, dashboard, array widget, widget e rect, con quattro chiamate `object` contemporanee.

Sono stati ricompilati gli stessi sorgenti con il GCC Xtensa della cache SDKv6.1 e `-fstack-usage`. La prima sonda isolata `-Os` riportava parse480byte/object128byte; **non è la configurazione effettiva della build IDF**, che usa `-Og`. Ripetendo la sonda con l’intera riga `compile_commands.json` della build IDF, aggiungendo soltanto `-fstack-usage` e cambiando il file oggetto di destinazione, risultano:

| Funzione | Frame statico riportato, build `-Og` |
|---|---:|
| `ag_config_json_parse` | 480 byte |
| `object` | 96 byte per chiamata |
| `value` | 80 byte per chiamata |
| `dashboards`, `widgets` | 48 byte ciascuna |
| `string` | 48 byte |
| Altri helper presenti nel `.su` | 32 byte ciascuno |

I frame **si sommano lungo le chiamate annidate**:480byte non è il consumo totale del parser. Queste sono informazioni statiche del compilatore, non una misura high-water del task e non comprendono automaticamente caller, stack SMP/BLE, libreria C, validatore, dispatcher o altre fasi. Serve misurazione del percorso completo e margine esplicito prima di un’integrazione runtime; lo stack default di `app_main` non è qualificato. Il dispatcher dovrà separare parsing/validazione da sessione autorizzata, expected revision e commit, senza assumere che un JSON valido autorizzi scritture.

## Ripetere la sonda senza cambiare il parser

Dopo `sh firmware/tools/build_esp32s3.sh`, dalla root:

```python
import json, pathlib, shlex, subprocess
build = pathlib.Path('firmware/idf/build').resolve()
item = next(i for i in json.load(open(build/'compile_commands.json'))
            if i['file'].endswith('/shared/c/ag_config_json.c'))
args = item.get('arguments') or shlex.split(item['command'])
args[args.index('-o') + 1] = str(build/'ag_config_json_actual_stack_probe.o')
args.append('-fstack-usage')
subprocess.run(args, cwd=item['directory'], check=True)
print((build/'ag_config_json_actual_stack_probe.su').read_text())
```

Le sonde restano soltanto nelle directory build ignorate; non vengono collegate al firmware. Il report dispositivo elenca separatamente `compiled_shared_sources` dai quattordici sorgenti `firmware/src`. La dimensione170624byte del `.bin` rimane quella dello skeleton: non dimostra memoria/stack del futuro parser operativo né sizing OTA.
