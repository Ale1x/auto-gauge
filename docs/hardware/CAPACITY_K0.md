# Sensibilità memoria e interrogazioni — K0, 2026-10-24

Generato da scripts/build_feasibility_models.py. Solo calcoli su ipotesi esplicite: nessuna build firmware, simulazione ECU, misura o scelta definitiva.

## Flash candidata da 16 MiB

Ipotesi: due slot OTA uguali; 2 MiB log approvati condizionatamente; riserva contabile256 KiB per strutture iniziali/configurazione; asset separati. Non è una partition table. La riserva non certifica il fit del bootloader firmato o delle configurazioni di sicurezza. Nessuna immagine factory aggiuntiva inclusa: se richiesta, ricalcolare.

| Slot singolo MiB | Asset MiB | Spazio residuo MiB | Fit solo aritmetico |
|---:|---:|---:|---|
| 3 | 1 | 6.75 | sì |
| 3 | 2 | 5.75 | sì |
| 3 | 4 | 3.75 | sì |
| 4 | 1 | 4.75 | sì |
| 4 | 2 | 3.75 | sì |
| 4 | 4 | 1.75 | sì |
| 5 | 1 | 2.75 | sì |
| 5 | 2 | 1.75 | sì |
| 5 | 4 | -0.25 | no |
| 6 | 1 | 0.75 | sì |
| 6 | 2 | -0.25 | no |
| 6 | 4 | -2.25 | no |

E34: ESP-IDF richiede allineamento64 KiB degli offset app e4 KiB delle partizioni; otadata occupa8 KiB nell’esempio ufficiale. Gli slot qui usano multipli interi di MiB. Verificare la tabella effettiva con il generatore ESP-IDF e il binario firmato completo, margine di crescita e configurazione bootloader. USB recovery non implica automaticamente una terza immagine factory residente.

## Log: effetto del formato

Ipotesi aggiuntive: blocchi4096 byte, header64 byte, due blocchi riservati al recupero. Sono valori di modello, non geometria flash verificata. Frequenza massima approvata1 Hz/PID. Il record16 byte non è garantito per tutti i payload Service01: prevedere formato variabile o segmentazione senza troncare dati; ricalcolare con il decoder reale.

| Record byte | PID | Durata ore | RAM per64 record byte | Tempo coda vuota→piena s |
|---:|---:|---:|---:|---:|
| 16 | 5 | 7.14 | 1024 | 12.80 |
| 16 | 15 | 2.38 | 1024 | 4.27 |
| 32 | 5 | 3.57 | 2048 | 12.80 |
| 32 | 15 | 1.19 | 2048 | 4.27 |
| 64 | 5 | 1.78 | 4096 | 12.80 |
| 64 | 15 | 0.59 | 4096 | 4.27 |

La colonna coda assume logger completamente fermo e produttore continuo; non autorizza uno stallo di quella durata. Metadati RAM aggiuntivi e record di sessione/perdita non inclusi. Retention reale può essere inferiore. Nessuna previsione di endurance senza MPN flash e write amplification.

E35: scrittura/erase flash può disabilitare cache e sospendere task; collocare il logger a bassa priorità non garantisce da solo UI/CAN continui. Verificare versione ESP-IDF, driver, memoria interna, ISR e tempi peggiori sul modulo esatto. Nessun affidamento implicito a opzioni sperimentali o PSRAM per eliminare gli stalli.

## CAN: latenza ECU e scheduler seriale

Modello H: una sola transazione pendente; tempo totale comprende invio, arbitraggio, attesa ECU e finestra di risposta. Non va sommato di nuovo al tempo ECU. Frequenza massima aritmetica perPID =1000/(N × tempo_ms). La capacità reale è inferiore per discovery, retry, risposte multiple e altri lavori. È uno studio di sensibilità, non una misura o un limite normativo.

| PID | Tempo totale ms | Massimo aritmetico Hz/PID | Occupazione seriale per5 Hz |
|---:|---:|---:|---:|
| 1 | 10 | 100.00 | 5% |
| 1 | 20 | 50.00 | 10% |
| 1 | 40 | 25.00 | 20% |
| 1 | 100 | 10.00 | 50% |
| 1 | 200 | 5.00 | 100% |
| 5 | 10 | 20.00 | 25% |
| 5 | 20 | 10.00 | 50% |
| 5 | 40 | 5.00 | 100% |
| 5 | 100 | 2.00 | 250% |
| 5 | 200 | 1.00 | 500% |
| 15 | 10 | 6.67 | 75% |
| 15 | 20 | 3.33 | 150% |
| 15 | 40 | 1.67 | 300% |
| 15 | 100 | 0.67 | 750% |
| 15 | 200 | 0.33 | 1500% |

Cinque PID a5 Hz richiedono in media meno di40 ms/transazione per lasciare margine; quindici richiedono meno di13,33 ms. Un caso H con quattro risposte da20 ms e un timeout da200 ms dura280 ms/giro: circa3,57 Hz per i quattro PID rispondenti; il quinto resta TIMEOUT, mai dato valido. L’occupazione del bus in bit/s da sola non prova5 Hz.

Candidato da verificare: coda bounded, priorità ai PID visibili, riduzione rate dei non visibili e backoff per ECU/PID in timeout. Registrare richieste, risposte complete, scadenze, errori e origine ECU; riportare frequenza effettiva. Nessun timeout numerico, soglia di bus load o criterio O-09 viene approvato da questo modello. Standby inibisce comunque le richieste.

## Condizioni per proseguire

O-14 resta aperta sul sizing: build firmata, asset/font reali, formato raw e benchmark flash/BLE/UI. O-09 resta aperta: fixture1/5/15PID, latenza e perdita, con e senza log/erase/sync/OTA. K0 non superato. CSV riproducibili in evidence/models/. Fonti E34/E35 in evidence/SOURCES.md.
