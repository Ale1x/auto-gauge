# Test di integrazione

Test Python di contratti, fault, sorgenti e interoperabilità con il core C. I driver C in questa directory sono usati dai casi Python. I template di acquisizione reale in [templates/](templates/README.md) non contengono misure eseguite.

```sh
python3 -m unittest discover -s tests -v
python3 -m unittest discover -s tests -p test_app_model_ux.py
```

`python3 scripts/check_host.py` aggiunge verifica dei contratti, build CMake e CTest. I test Swift del package sono in `mobile/Tests/`, i flussi UI in `mobile/UITests/`, i test firmware in `firmware/tests/`.

Le evidenze UI sono in [docs/validation/](../docs/validation/README.md); i risultati storici in [docs/verification/](../docs/verification/README.md).
