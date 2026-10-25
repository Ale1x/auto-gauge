#!/usr/bin/env python3
"""Reproduce K0 sensitivity calculations; no firmware or hardware validation."""
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'evidence' / 'models'
OUT.mkdir(exist_ok=True)
MIB = 1024 ** 2

def write_csv(name, fields, rows):
    with (OUT / name).open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

# All allocations are hypotheses. The reserved region includes early flash
# structures and configuration; actual bootloader/security requirements are TBD.
flash = []
for slot_mib in (3, 4, 5, 6):
    for assets_mib in (1, 2, 4):
        used = 256 * 1024 + 2 * slot_mib * MIB + 2 * MIB + assets_mib * MIB
        flash.append(dict(slot_mib=slot_mib, assets_mib=assets_mib,
                          log_mib=2, reserved_kib=256,
                          remaining_mib=(16 * MIB - used) / MIB,
                          fits_arithmetic_only=used <= 16 * MIB))
write_csv('flash_sensitivity.csv', list(flash[0]), flash)

# Hypothetical 4 KiB blocks, 64-byte headers and two recovery blocks.
# Record sizes include record metadata; final raw PID format is not selected.
logs = []
for record_bytes in (16, 32, 64):
    blocks = 2 * MIB // 4096 - 2
    capacity = blocks * ((4096 - 64) // record_bytes)
    for pid_count in (5, 15):
        logs.append(dict(record_bytes=record_bytes, pid_count=pid_count,
                         hz_per_pid=1, usable_records=capacity,
                         retention_hours=capacity / pid_count / 3600,
                         queue_64_records_bytes=64 * record_bytes,
                         queue_empty_to_full_seconds=64 / pid_count))
write_csv('log_sensitivity.csv', list(logs[0]), logs)

# Serial, one outstanding diagnostic transaction. Total service time includes
# bus, arbitration, ECU wait and response window. These are sensitivity inputs.
can = []
for n in (1, 5, 15):
    for service_ms in (10, 20, 40, 100, 200):
        can.append(dict(pid_count=n, transaction_total_ms=service_ms,
                        serial_ceiling_hz_per_pid=1000 / (n * service_ms),
                        target_hz_per_pid=5,
                        utilization_for_target=n * 5 * service_ms / 1000,
                        below_full_utilization=n * 5 * service_ms < 1000))
write_csv('can_latency_sensitivity.csv', list(can[0]), can)

lines = ['# Sensibilità memoria e interrogazioni — K0, 2026-10-24', '',
         'Generato da scripts/build_feasibility_models.py. Solo calcoli su ipotesi esplicite: nessuna build firmware, simulazione ECU, misura o scelta definitiva.', '',
         '## Flash candidata da 16 MiB', '',
         'Ipotesi: due slot OTA uguali; 2 MiB log approvati condizionatamente; riserva contabile256 KiB per strutture iniziali/configurazione; asset separati. Non è una partition table. La riserva non certifica il fit del bootloader firmato o delle configurazioni di sicurezza. Nessuna immagine factory aggiuntiva inclusa: se richiesta, ricalcolare.', '',
         '| Slot singolo MiB | Asset MiB | Spazio residuo MiB | Fit solo aritmetico |',
         '|---:|---:|---:|---|']
for r in flash:
    lines.append(f"| {r['slot_mib']} | {r['assets_mib']} | {r['remaining_mib']:.2f} | {'sì' if r['fits_arithmetic_only'] else 'no'} |")
lines += ['', 'E34: ESP-IDF richiede allineamento64 KiB degli offset app e4 KiB delle partizioni; otadata occupa8 KiB nell’esempio ufficiale. Gli slot qui usano multipli interi di MiB. Verificare la tabella effettiva con il generatore ESP-IDF e il binario firmato completo, margine di crescita e configurazione bootloader. USB recovery non implica automaticamente una terza immagine factory residente.', '',
          '## Log: effetto del formato', '',
          'Ipotesi aggiuntive: blocchi4096 byte, header64 byte, due blocchi riservati al recupero. Sono valori di modello, non geometria flash verificata. Frequenza massima approvata1 Hz/PID. Il record16 byte non è garantito per tutti i payload Service01: prevedere formato variabile o segmentazione senza troncare dati; ricalcolare con il decoder reale.', '',
          '| Record byte | PID | Durata ore | RAM per64 record byte | Tempo coda vuota→piena s |',
          '|---:|---:|---:|---:|---:|']
for r in logs:
    lines.append(f"| {r['record_bytes']} | {r['pid_count']} | {r['retention_hours']:.2f} | {r['queue_64_records_bytes']} | {r['queue_empty_to_full_seconds']:.2f} |")
lines += ['', 'La colonna coda assume logger completamente fermo e produttore continuo; non autorizza uno stallo di quella durata. Metadati RAM aggiuntivi e record di sessione/perdita non inclusi. Retention reale può essere inferiore. Nessuna previsione di endurance senza MPN flash e write amplification.', '',
          'E35: scrittura/erase flash può disabilitare cache e sospendere task; collocare il logger a bassa priorità non garantisce da solo UI/CAN continui. Verificare versione ESP-IDF, driver, memoria interna, ISR e tempi peggiori sul modulo esatto. Nessun affidamento implicito a opzioni sperimentali o PSRAM per eliminare gli stalli.', '',
          '## CAN: latenza ECU e scheduler seriale', '',
          'Modello H: una sola transazione pendente; tempo totale comprende invio, arbitraggio, attesa ECU e finestra di risposta. Non va sommato di nuovo al tempo ECU. Frequenza massima aritmetica perPID =1000/(N × tempo_ms). La capacità reale è inferiore per discovery, retry, risposte multiple e altri lavori. È uno studio di sensibilità, non una misura o un limite normativo.', '',
          '| PID | Tempo totale ms | Massimo aritmetico Hz/PID | Occupazione seriale per5 Hz |',
          '|---:|---:|---:|---:|']
for r in can:
    lines.append(f"| {r['pid_count']} | {r['transaction_total_ms']} | {r['serial_ceiling_hz_per_pid']:.2f} | {100*r['utilization_for_target']:.0f}% |")
lines += ['', 'Cinque PID a5 Hz richiedono in media meno di40 ms/transazione per lasciare margine; quindici richiedono meno di13,33 ms. Un caso H con quattro risposte da20 ms e un timeout da200 ms dura280 ms/giro: circa3,57 Hz per i quattro PID rispondenti; il quinto resta TIMEOUT, mai dato valido. L’occupazione del bus in bit/s da sola non prova5 Hz.', '',
          'Candidato da verificare: coda bounded, priorità ai PID visibili, riduzione rate dei non visibili e backoff per ECU/PID in timeout. Registrare richieste, risposte complete, scadenze, errori e origine ECU; riportare frequenza effettiva. Nessun timeout numerico, soglia di bus load o criterio O-09 viene approvato da questo modello. Standby inibisce comunque le richieste.', '',
          '## Condizioni per proseguire', '',
          'O-14 resta aperta sul sizing: build firmata, asset/font reali, formato raw e benchmark flash/BLE/UI. O-09 resta aperta: fixture1/5/15PID, latenza e perdita, con e senza log/erase/sync/OTA. K0 non superato. CSV riproducibili in evidence/models/. Fonti E34/E35 in evidence/SOURCES.md.', '']
(ROOT / 'docs/hardware/CAPACITY_K0.md').write_text('\n'.join(lines))
print(f'Generated {len(flash)} flash, {len(logs)} log, {len(can)} CAN sensitivity cases; no hardware validation.')
