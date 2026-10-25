#!/usr/bin/env python3
"""LM5164-Q1 analytical screening; never a stability or hardware PASS."""
import csv
import itertools
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'evidence/models'


def point(vin, vout, ron, ra, ca, inductance, cout):
    # E04 Rev.C equations 15, 18, 21, 24, 25. SI units throughout.
    fsw = vout * 2.5e9 / ron
    ton = vout / (vin * fsw)
    ramp = ton * (vin - vout) / (ra * ca)
    delta_i = vout / (fsw * inductance) * (1 - vout / vin)
    return dict(fsw_Hz=fsw, ton_s=ton, injected_ripple_V=ramp,
                inductor_ripple_A=delta_i,
                capacitive_output_ripple_V=delta_i / (8 * fsw * cout),
                peak_at_1A_A=1 + delta_i / 2,
                load_before_min_peak_limit_A=1.25 - delta_i / 2)


def main():
    # Independent published example: E04 §7.2, 48V -> 12V, ~300kHz,
    # RA453k/CA3.3nF, L68uH. Detect unit/conversion errors in the formulas.
    reference = point(48, 12, 100e3, 453e3, 3.3e-9, 68e-6, 22e-6)
    assert abs(reference['fsw_Hz'] - 300e3) < 1
    assert abs(reference['injected_ripple_V'] - .020) < .0002
    assert abs(reference['inductor_ripple_A'] - .4412) < .001

    configs = [('nominal_12V', 200e3, 2.2e-9),
               ('low_input_study', 100e3, 1.8e-9)]
    voltages = (6, 7, 8, 9, 12, 14.4, 18, 40)
    rows = []
    for name, ra, ca in configs:
        for vin in voltages:
            nominal = point(vin, 4.992, 41.2e3, ra, ca, 47e-6, 22e-6)
            # Passive sensitivity, NOT selected part tolerances. No device
            # on-time process bound is provided by the typical timing entries.
            # Thus even these extrema are conditional on the ideal timing law.
            corners = []
            for ref, top, bottom, rr, rra, cca in itertools.product(
                    (1.181, 1.218), (.99, 1.01), (.99, 1.01),
                    (.99, 1.01), (.99, 1.01), (.95, 1.05)):
                vout = ref * (1 + 316e3 * top / (100e3 * bottom))
                p = point(vin, vout, 41.2e3 * rr, ra * rra,
                          ca * cca, 47e-6 * .8, 22e-6)
                parallel = 1 / (1 / (316e3 * top) + 1 / (100e3 * bottom))
                p['ca_min_F'] = 10 / (p['fsw_Hz'] * parallel)
                p['ca_margin'] = ca * cca / p['ca_min_F']
                corners.append(p)
            rows.append(dict(configuration=name, vin_at_buck_V=vin,
                             ra_ohm=ra, ca_F=ca, cb_study_F=100e-12,
                             effective_cout_assumption_F=22e-6,
                             **nominal,
                             ripple_corner_min_V=min(p['injected_ripple_V'] for p in corners),
                             ripple_corner_max_V=max(p['injected_ripple_V'] for p in corners),
                             ca_margin_min=min(p['ca_margin'] for p in corners),
                             peak_at_1A_corner_max_A=max(p['peak_at_1A_A'] for p in corners),
                             reaches_12mV_in_ideal_corners=all(p['injected_ripple_V'] >= .012 for p in corners),
                             status='ANALYTICAL_SCREEN_ONLY_NOT_VALIDATED'))
    OUT.mkdir(exist_ok=True)
    with (OUT / 'buck5_ripple_screen.csv').open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    summary = dict(
        status='NO_CONFIGURATION_QUALIFIED', configurations=len(configs),
        operating_points=len(rows), corners_per_point=64,
        source='E04 LM5164-Q1 Rev.C; E52 SNVA874 March2019',
        ideal_divider_V=4.992,
        divider_reference_tolerance_V=[1.181 * (1 + 316e3 * .99 / (100e3 * 1.01)),
                                      1.218 * (1 + 316e3 * 1.01 / (100e3 * .99))],
        ca_min_nominal_F=10 / ((4.992 * 2.5e9 / 41.2e3) * (316e3 * 100e3 / 416e3)),
        cb_min_for_assumed_75us_F=75e-6 / (3 * 316e3),
        limitations=['CCM ideal timing, not DEM/pulse-skipping or dropout',
                     'No process bound for on-time, no phase-margin calculation',
                     'Cout effective22uF and passive tolerances are assumptions',
                     'VIN40V is protected-node sensitivity, not a nominal24V requirement or a surge test',
                     'No selected MPN/footprints, source verification, simulation or bench test'])
    (OUT / 'buck5_ripple_screen.json').write_text(json.dumps(summary, indent=2) + '\n')
    lines = [
        '# Buck 5 V — screening del feedback LM5164-Q1', '',
        '2026-10-24. Generato da `scripts/build_buck5_screen.py`. Studio di due reti alternative; nessuna scelta definitiva di componenti o dichiarazione di stabilità.', '',
        '## Risultato', '',
        'La rete studiata per circa20mV a12V non mantiene12mV quando la tensione al buck scende verso6V. La seconda aumenta il ripple a bassa tensione, ma introduce un errore DC maggiore. Nessuna delle due è qualificata: R210/C208/C209 restanoTBD nello schema.', '',
        '| Rete di studio | VIN al buck V | Ripple FB nominale mV | Minimo nei corner ideali mV | Picco induttore a1A, corner A |',
        '|---|---:|---:|---:|---:|']
    for r in rows:
        if r['vin_at_buck_V'] in (6, 8, 12, 18, 40):
            lines.append(f"| {r['configuration']} | {r['vin_at_buck_V']} | {1e3*r['injected_ripple_V']:.2f} | {1e3*r['ripple_corner_min_V']:.2f} | {r['peak_at_1A_corner_max_A']:.3f} |")
    lines += ['', '## Metodo e ipotesi', '',
              'FonteE04: datasheetTI LM5164-Q1 Rev.C, equazioni15/18/21/24/25/26. RON41,2kΩ, partitore316kΩ/100kΩ, L47µH: valori inizialiA0. VOUT ideale4,992V e FSW≈303kHz. VIN è misurata sul pin2 del buck dopo cavo/protezioni:6V qui non sono6V alla presaOBD.', '',
              'Rete nominal_12V: RA200kΩ, CA2,2nF. Rete low_input_study: RA100kΩ, CA1,8nF. CB100pF solo per studio, con ipotesi75µs per la formula26; non garantisce quel tempo di risposta. Nessun MPN selezionato e nessuna disponibilità verificata per questi passivi.', '',
              'FSW=VOUT×2,5·10⁹/RON; tON=VOUT/(VIN×FSW); ΔVFB≈tON×(VIN−VOUT)/(RA×CA); ΔIL=VOUT×(1−VOUT/VIN)/(FSW×L). COUTeff=22µF è un’ipotesi per il condensatore nominale47µF, non una curva DC-bias verificata. Ripple capacitivo=ΔIL/(8×FSW×COUTeff); non includeESR, ringing o transitori di carico.', '',
              '64corner per punto: VREF1,181/1,218V (limiti datasheet), quattro resistenze ±1% e CA±5% (ipotesi). Per il picco si usa L ridotta del20% (ipotesi). Questi estremi non includono dispersione del timing interno, temperatura dei passivi, ESR/DCR, perditeFET o bias dei condensatori. La tabella timingTI fornisce valori tipici: non trasformare il calcolo in una garanzia worst-case.', '',
              f"Il solo partitore, includendo i limitiVREF e resistenze±1%, dà {summary['divider_reference_tolerance_V'][0]:.3f}…{summary['divider_reference_tolerance_V'][1]:.3f}V prima dell’errore del ripple. CAmin nominale={summary['ca_min_nominal_F']*1e12:.0f}pF; CBmin per75µs={summary['cb_min_for_assumed_75us_F']*1e12:.1f}pF.", '',
              '## Vincoli prima di scegliere la rete', '',
              'TI raccomanda circa20mV al punto nominale e almeno12mV all’ingresso minimo. Per la rete low_input_study il ripple nominale a12V è circa54mV: la stima di errore DC è metà ripple moltiplicata per4,16, cioè circa112mV in uscita. Non sommare questa stima ai limiti statici per dichiarare una finestra garantita. Occorre risolvere il punto di lavoro reale e verificare compatibilitàTCAN/mux/rail3,3V.', '',
              'E52 mostra che ampiezza del ripple da sola non dimostra stabilità: il ripple capacitivo d’uscita sfasato può dominare il feedback. Servono analisi completa della rete, simulazione e prove di step carico/linea, DEM e avvio. Anche il superamento dello screening12mV resta solo un controllo numerico preliminare.', '',
              'Il picco va confrontato con il minimo limiteHS1,25A; Isat deve invece coprire il massimo limite1,75A più overshoot/delay e derating, come richiesto dal datasheet. Il modello non seleziona l’induttore. A40V il calcolo esamina sensibilità del nodo protetto: non autorizza uso nominale24V e non definisce il clamp automotive.', '',
              'La continuità a6V non è dimostrata: dropout, cadute e limitazione di corrente possono invalidare la leggeCCM ideale. DEC-008 consente reset al crank; soglieUV/OV e recupero automatico richiedono progetto separato.', '',
              '## Riproduzione e chiusura', '',
              '`python3 scripts/build_buck5_screen.py` rigenera questo documento, CSV eJSON. Una verifica numerica riproduce l’esempio pubblicatoTI48V→12V (300kHz, ~20mV, ~0,44A ripple) per intercettare errori di unità nelle equazioni.', '',
              'Prima di assegnare valori nello schema: fissare la finestra del nodo protetto insieme al wake/UVLO; verificare il budget5V reale; scegliereMPN e curve dei passivi; eseguire analisi di stabilità e bench. Nessun valoreTBD è stato sostituito con una falsa scelta finale.', '',
              'Fonti: [E04 datasheetTI](https://www.ti.com/lit/ds/symlink/lm5164-q1.pdf), [E52 notaTI SNVA874](https://www.ti.com/lit/an/snva874/snva874.pdf).', '']
    (ROOT / 'docs/hardware/BUCK5_FEEDBACK_REVIEW.md').write_text('\n'.join(lines))
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
