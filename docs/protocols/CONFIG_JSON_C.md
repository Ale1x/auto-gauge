# Parser C bounded della configurazione wire — sviluppo v0.1

24 ottobre 2026. Implementazione in `shared/c/ag_config_json.h` e `.c`. Usa i tipi di `firmware/include/ag_core.h` e la validazione `firmware/src/config.c`, senza dipendenze esterne o sorgenti vendorizzati. È un parser di configurazione schema1, non un parser RPC, un trasporto autenticato o un commit persistente.

## API e garanzie

```c
ag_config_json_error_t error;
ag_config_t candidate;
ag_config_json_result_t result = ag_config_json_parse(
    bytes, byte_count, &supported_pids, &candidate, &error);
```

Il chiamante fornisce input length-delimited e oggetti validi, senza modificarli concorrentemente. Non serve NUL finale; un NUL compreso nella lunghezza non è whitespace JSON e viene rifiutato. `error` è facoltativo. Il parser restituisce una configurazione candidata solo su `AG_CONFIG_JSON_OK`; su qualsiasi errore l'oggetto `candidate` resta **byte per byte immutato**. Non conserva puntatori all'input. Offset di errore mai oltre la lunghezza; il dettaglio `validation` descrive l'errore del core quando il risultato è `CONFIG_INVALID`.

Dopo parsing, `ag_config_validate` verifica schema, revisione, brightness, limiti3×5, ID, geometria e capability PID passate dal chiamante. Il catalogo demo non è un'autorità incorporata. Una maschera sintetica con tutti i bit abilita soltanto i test strutturali: in runtime devono provenire dalla discovery qualificata del veicolo e dall'insieme dei decoder effettivamente implementati. Un risultato valido non autorizza apply, TX CAN o scrittura flash; restano necessari sessione, revisione attesa e percorso di commit appropriati.

## Sottoinsieme JSON esplicito

Il parser riconosce l'intera struttura e i separatori della configurazione normale in `fixtures/config.valid.json`: root, brightness, dashboard, widget e rect. Ordine delle proprietà libero; ogni proprietà prevista è obbligatoria. Non è un parser generico per qualunque documento JSON.

- **Input massimo4096byte**, compresi whitespace;4097 viene rifiutato prima della lettura. Whitespace ammesso soloSP/TAB/CR/LF. Un solo valore root object; niente trailing data, BOM, commenti o trailing comma.
- **Chiavi sconosciute rifiutate a ogni livello**, anziché proiettate/scartate. È intenzionalmente più restrittivo di alcune implementazioni storiche Swift/Python che ignorano campi extra. Non chiamarlo interoperabilità dimostrata per documenti con estensioni: il normale schema e i22casi comuni sono verificati. Un cambio di schema richiede aggiornamento esplicito.
- **Duplicati rifiutati a ogni livello**, dopo decodifica delle chiavi. `x` e `\u0078` sono la stessa chiave; un secondo nome equivalente non passa. Il controllo è locale all'oggetto: gli stessi nomi in oggetti diversi restano leciti.
- **Stringhe solo ASCII stampabile U+0020…U+007E**, fino31byte decodificati. Supportati escape JSON per virgolette, backslash e slash, e `\uXXXX` quando decodifica nello stesso intervallo ASCII. Unicode non ASCII, surrogate anche accoppiate, controlli raw/escaped e stringhe più lunghe sono rifiutati con errore esplicito, non troncati o normalizzati. Tutte le chiavi e i valori enum normali sono nel sottoinsieme. Esempio `"man\u0075al"` equivale a `"manual"`.
- **Campi numerici solo token interi decimali signed32**, con grammatica JSON: niente `+`, zeri iniziali, float, esponenti, booleani, null o numeri in stringhe. Bound lessicale−2147483648…2147483647, poi vincoli semantici del core. `-0` è un intero JSON ammesso e produce0; contatori unsigned64 degli storici non appartengono a questa API.
- `brightness.mode` accetta esclusivamente `manual` e `auto`; mantiene `manual_percent` anche in auto. Non aggiunge curve o policy di luminosità.

## Limiti di memoria e complessità

Nessuna allocazione heap, VLA o copia dell'intero input. Stato del parser, una configurazione candidata e buffer stringa da32byte; chiamate annidate secondo il solo schema noto. Contatori espliciti limitano profondità a8container e token a512 (apertura container, chiave o scalare). Le cardinalità3dashboard×5widget vengono controllate **prima** dell'accesso agli array. Il normale schema raggiunge al massimo6container annidati; una struttura più profonda è generalmente rifiutata già per tipo non previsto, senza ricorrere su input arbitrario. Non sono limiti che l'input possa alzare.

Ogni byte viene esaminato un numero limitato di volte e l'input è comunque≤4096byte. Le tabelle di chiavi hanno massimo4nomi, le maschere duplicati sono locali. La conversione numerica verifica il limite prima di moltiplicare/aggiungere; coordinate fuori range vengono respinte dal core prima delle somme geometriche. La memoria stack effettiva sul target va misurata nella build/runtime; nessuna promessa di stack ESP-IDF viene dedotta dalla prova host.

## Prove riproducibili

```sh
python3 -m unittest discover -s tests -p test_c_config_json.py -v
```

La suite compila il parser insieme al validatore core e a `tests/config_json_driver.c` con `-Wall -Wextra -Wpedantic -Werror`, ASan e UBSan in directory temporanea. Il driver verifica l'immutabilità byte per byte dell'output su ogni rifiuto e l'offset bounded.

Copertura: golden normale e tutti22casi condivisi; massima configurazione3×5 con revisioneINT32_MAX ePID fuori demo; maschera capability vuota; input4096/4097byte; NUL/BOM/trailing data; proprietà mancanti/extra/duplicate in ogni oggetto, incluse equivalenze `\u`; escapeASCII e rifiuto Unicode/controlli; overflow/interi enormi/frazioni/esponenti/tipi errati; separatori/container malformati; ordine libero/whitespace; ogni troncatura del golden compatto, mutazione di ogni byte conNUL e0xFF e1024input pseudo-casuali bounded. Queste sono prove deterministiche host, non fuzzing esaustivo o validazione del canale BLE.

Nessuna modifica a firmware, mobile o contratti/fixture esistenti. Nessuna radio, flash, cloud, chiave o provisioning attivata. Il parser è pronto per essere collegato e riesaminato nel futuro dispatcher, non un'autorizzazione implicita a farlo operare sul veicolo.
