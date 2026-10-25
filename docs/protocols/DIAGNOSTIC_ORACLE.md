# Oracle C per il catalogo diagnostico mobile

24 ottobre 2026. `fixtures/diagnostic-scalars.cases.json` contiene212vettori:4raw per ognuno dei53PID scalari implementati. Il generatore compila ed esegue **il vero `ag_obd_parse_measurement`**, con ASan/UBSan, enumerando l’allowlist e provando le lunghezze consentite. Non ricava le risposte copiando i coefficienti C in un altro linguaggio.

Raw minimo, bit alto, massimo e pattern non uniforme esercitano1/2/4byte big-endian, frazioni, offset e U32. `unit_code` usa i valori stabili append-only di `ag_unit_t`. Il file è fixture di test, non un catalogo capability del veicolo e non entra nei dati live dell’app.

```sh
python3 scripts/generate_diagnostic_oracle.py          # rigenerazione esplicita
python3 scripts/generate_diagnostic_oracle.py --check  # nessuna modifica
python3 -m unittest discover -s tests -p test_diagnostic_oracle.py -v
```

I test Swift leggono la fixture readonly e confrontano formule, unità e lunghezze del catalogo app. Il check Python in CI rigenera temporaneamente l’output e confronta il file salvato byte per byte, così una modifica al decoder non lascia un oracle vecchio apparentemente verde. Errori/lunghezze/status sono verificati anche nelle suite C/Swift dedicate.

L’accordo fra C e Swift verifica la coerenza delle implementazioni, non certifica nuove formule SAE o disponibilità su una ECU specifica. Restano valide fonti e omissioni del catalogo [firmware](../firmware/DIAGNOSTIC_COVERAGE.md). Nessuna richiesta CAN o connessione al veicolo viene eseguita dal generatore.
