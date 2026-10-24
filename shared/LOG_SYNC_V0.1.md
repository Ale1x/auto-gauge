# Import storico — riferimento locale v0.1

24 ottobre 2026. Modulo eseguibile `reference/log_import.py`, verificato con SQLite su host. Non è collegato alla UI iPhone né alla flash del gauge. Il trasporto BLE autenticato resta da implementare; questo riferimento riceve pagine di dati già decodificati da una sorgente di test.

## Pagina e limiti di sviluppo

Un device identifica la sorgente; una sessione è una stringa di32 cifre esadecimali minuscole non nulla, coerente con l'identificatore di boot128bit. Una pagina JSON contiene `session_id`, `records` e `lost`; dimensione serializzata compatta fino a4096byte, fino a16record e16intervalli di perdita. Il futuro envelope wire deve aggiungere i propri metadati senza superare il limite del payload: il massimo del corpo qui non concede altri byte oltre4096.

Ogni record contiene `sequence` e `monotonic_ms` come stringhe decimali canoniche unsigned64; non usare double né convertire in data assoluta senza un'ancora. Sequenza parte da1, tempo da0; nessuna precisione persa su JSON/JavaScript. Gli altri campi sono `pid` (1–255, Service01 implicito), `ecu_source` (identificatore CAN fino a29bit), `status` e `raw_hex`.

Stati: `valid`, `unsupported`, `unavailable`, `stale`, `timeout`, `error`. Solo valid ha un raw non vuoto. Il riferimento accetta fino a64byte raw per evoluzione del protocollo; il logger firmware attuale resta limitato a8byte e rifiuta payload più grandi. Nessun campione timeout/stale riusa un vecchio valore come attuale. Il modulo non verifica il decoder specifico del PID o la provenienza ECU.

## Deduplica e ACK

Chiave univoca: device, sessione, sequenza. Una ripetizione identica è idempotente; lo stesso identificatore con contenuto diverso causa errore e rollback dell'intera pagina. Record ordinati in modo strettamente crescente. Sessioni diverse non condividono il cursore.

Il cursore può avanzare soltanto attraverso record ricevuti oppure intervalli esatti di perdita dichiarati. Un buco non giustificato rifiuta la pagina. `lost` contiene coppie `first`/`last` decimali, ordinate internamente e prive di overlap nella pagina. Non usare direttamente l'inviluppo globale `first_dropped_sequence`…`last_dropped_sequence` del logger come se provasse che ogni sequenza intermedia sia persa: il consumer può averne già importate alcune. Il futuro produttore delle pagine deve determinare intervalli esatti o negoziare un reset esplicito del cursore.

Record, perdite e cursore sono salvati nella stessa transazione SQLite. `BEGIN IMMEDIATE` precede la lettura del cursore e serializza anche due importatori distinti sullo stesso archivio. L'ACK `acknowledged_through` viene restituito solo dopo il commit locale; se fallisce validazione/scrittura, nessun ACK nuovo. Una perdita non può contraddire record già importati e un record tardivo non può sostituire silenziosamente un intervallo già riconosciuto come perso. I test riaprono l'archivio per verificare il resume dopo riavvio e simulano l'interleaving di due connessioni.

Ogni connessione ha un singolo owner; il database serializza importatori su connessioni distinte e può restituire un errore di lock, da ritentare senza ACK. Le chiavi e le lunghezze primitive vengono verificate prima della serializzazione della pagina, evitando nuove allocazioni proporzionali a campi extra non limitati. Il riferimento non limita la retention totale dell'archivio sul telefono e non sceglie la policy cloud O-13. La memoria del gauge resta un tema separato. SQLite host non equivale a protezione dei dati iOS, backup, spazio disco garantito o prova di durabilità a ogni interruzione elettrica.

## Simulatore di configurazione

`reference/simulator.py` riceve packet del framing di sviluppo, con JSON UTF-8. Rifiuta chiavi duplicate, nonfinite e messaggi non validi. Implementa solo `hello`, `config.get`, `config.validate`, `config.apply` su stato simulato in RAM. Tutte le risposte riportano `simulation=true`; l'identità è `SIMULATO-001`, mai un gauge reale.

`config.apply` emette due messaggi distinti: `received`, poi `applied` oppure `rejected`. Applicare richiede una configurazione valida/capability supportata, `expected_revision` corrispondente e nuova revisione uguale alla precedente+1. I campi extra vengono scartati proiettando il modello schema1, come fa Swift Codable; prima dell'applicazione viene verificato che lo stato normalizzato sia rileggibile entro il framing. L'ACK di applicazione dichiara `persistence=volatile`; non significa persistenza flash. OTA, ownership e CAN raw restano `not_implemented`. Non esistono listener di rete, GATT, credenziali, cifratura o antireplay.

## Verifica

`python3 -m unittest discover -s tests -p 'test_protocol_*.py' -v` controlla framing, interoperabilità C/Python con ASan/UBSan, frammentazione, timeout, config simulata, deduplica/resume e rollback. I vettori del framing sono usati anche dai test Swift. Queste sono prove di componenti e contratto: non un roundtrip con un dispositivo fisico o un trasporto BLE reale.
