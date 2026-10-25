# Acquisizioni HW-IGN

Il CSV contiene solo intestazione: nessuna misura eseguita. Copiarlo in una cartella di run insieme ai raw trace e ai metadati descritti in docs/hardware/HW_IGN_TEST_PROTOCOL.md. Corrente in ampere e tempo monotono in secondi; non confondere mA/µA. UNKNOWN è obbligatorio se il quadro non è annotato. Non modificare i template per simulare un PASS.

Analisi preliminare: `python3 scripts/analyze_hw_ign.py /percorso/run/hw_ign_samples.csv`. Lo strumento stampa JSON senza modificare l’acquisizione: conta campioni, mostra range e intervallo massimo fra campioni, segnala correnti campionate oltre100µA dopo300s da una transizione ON→OFF osservata e variazioni dei contatori di richieste negli intervalli OFF tardivi. Non attribuisce un orario OFF a una cattura che comincia già OFF o a una transizione UNKNOWN→OFF.

Timestamp devono crescere; numeri non finiti, CSV malformati e reset/wrap dei contatori vengono rifiutati. Separare i run in caso di reset. Usare0/1/UNKNOWN per display_on. Range di tensione e corrente sono osservazioni, non limiti approvati. La transizione annotata è campionata e richiede verifica della sincronizzazione con l’evento reale.

Il risultato resta sempre NOT_DETERMINED: né campioni tutti bassi, né template vuoto dimostrano il massimo100µA, il rispetto a12V, assenza di picchi o la conformità della procedura. Controllare raw trace, banda, incertezza, temperatura e metadati. I test in tests/test_hw_ign_analyzer.py usano dati sintetici temporanei esclusivamente per verificare lo strumento; non sono acquisizioni del gauge.
