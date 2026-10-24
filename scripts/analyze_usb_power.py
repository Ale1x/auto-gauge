#!/usr/bin/env python3
"""HW-02 USB/protected-wake sensitivity calculations; no hardware PASS."""
import csv
import hashlib
import itertools
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'hardware/usb_power'


def write_csv(name, rows):
    with (OUT / name).open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def divider_bounds(ref_min, ref_max, rt, rb, tolerance, leakage):
    # Positive input leakage sinks current from the resistor midpoint.
    values = [ref * (1 + top / bottom) + leak * top
              for ref, top, bottom, leak in itertools.product(
                  (ref_min, ref_max), (rt * (1 - tolerance), rt * (1 + tolerance)),
                  (rb * (1 - tolerance), rb * (1 + tolerance)), (-leakage, leakage))]
    return min(values), max(values)


def main():
    OUT.mkdir(exist_ok=True)
    assert divider_bounds(1, 1, 4000, 1000, 0, 0) == (5, 5)
    assert divider_bounds(1, 1, 4000, 1000, 0, 1e-6) == (4.996, 5.004)
    devices = [dict(mpn='TPS259631DDAR', vref_min=1.17, vref_max=1.22,
                    vref_f_min=1.08, vref_f_max=1.13, rt=3490., rb=1000.,
                    delay_typ_us=1.3, reverse_blocking_specified=False),
               dict(mpn='TPS25940AQRVCRQ1', vref_min=.97, vref_max=1.01,
                    vref_f_min=.9, vref_f_max=.94, rt=4430., rb=1000.,
                    delay_typ_us=2., reverse_blocking_specified=True)]
    ovp = []
    for device in devices:
        for tolerance in (.01, .001, .0001):
            lower, upper = divider_bounds(device['vref_min'], device['vref_max'],
                                           device['rt'], device['rb'], tolerance, .1e-6)
            fall_low, fall_high = divider_bounds(device['vref_f_min'], device['vref_f_max'],
                                                 device['rt'], device['rb'], tolerance, .1e-6)
            ovp.append(dict(mpn=device['mpn'], top_ohm=device['rt'], bottom_ohm=device['rb'],
                            resistor_tolerance=tolerance, leakage_abs_A=.1e-6,
                            trip_min_V=lower, trip_max_V=upper,
                            release_min_V=fall_low, release_max_V=fall_high,
                            static_pass_5p25_to_5p5=lower > 5.25 and upper < 5.5,
                            pass_entire_5p5_input_range=False,
                            margin_to_mux_5p5_V=5.5 - upper,
                            delay_typ_us=device['delay_typ_us'],
                            delay_max_specified=False,
                            status='NOT_QUALIFIED_DYNAMIC_MAXIMUM_MISSING'))
    write_csv('ovp_thresholds.csv', ovp)

    charge = []
    for current, delay_us, capacitance in itertools.product((.1, .5, 1, 2), (1.3, 2, 10), (1e-6, 22e-6, 47e-6)):
        charge.append(dict(excess_current_H_A=current, response_H_us=delay_us,
                           capacitance_H_F=capacitance,
                           output_rise_H_V=current * delay_us * 1e-6 / capacitance,
                           status='SENSITIVITY_ONLY_NOT_CURRENT_OR_DELAY_BOUND'))
    write_csv('fault_charge_sensitivity.csv', charge)

    # Cdvdt47nF +/-10%, table IDVDT/GDVDT min/max of TPS2596.
    ramp_min = 1.89e-6 * 20.31 / (47e-9 * 1.1)
    ramp_max = 2.33e-6 * 21.5 / (47e-9 * .9)
    inrush = []
    for capacitance in (1e-6, 22e-6, 47e-6, 100e-6):
        inrush.append(dict(cout_nominal_H_F=capacitance, cout_tolerance_H=.2,
                           cdvdt_H_F=47e-9, slew_min_V_per_s=ramp_min,
                           slew_max_V_per_s=ramp_max,
                           current_max_H_A=capacitance * 1.2 * ramp_max,
                           ramp_5V_min_s=5 / ramp_max, ramp_5V_max_s=5 / ramp_min,
                           excludes_load_and_cascaded_regulator_startup=True))
    write_csv('inrush_screen.csv', inrush)

    current_limits = []
    for resistance in (7870, 3830, 2100):
        # EUSB01 Eq.5: R=903/(Ilim-.0112). Current accuracy ±10.4%
        # is deliberately conservative across this exploratory current range.
        current_limits.append(dict(mpn='TPS259631DDAR', resistor_H_ohm=resistance,
                                   resistor_tolerance_H=.001,
                                   nominal_A=.0112 + 903 / resistance,
                                   minimum_screen_A=(.0112 + 903 / (resistance * 1.001)) * .896,
                                   maximum_screen_A=(.0112 + 903 / (resistance * .999)) * 1.104,
                                   current_consumption_not_included=True))
    write_csv('current_limit_screen.csv', current_limits)

    # Cascaded USB buck study: numerical efficiencies, DCR and low-VIN RON
    # are hypotheses, not guaranteed curves. Divider tolerance0.1% each.
    prereg = []
    for nominal in (4.2, 4.5):
        rt = (nominal / .8 - 1) * 10000
        lo, hi = divider_bounds(.8 * .97, .8 * 1.04, rt, 10000, .001, .4e-6)
        prereg.append(dict(nominal_H_V=nominal, rtop_H_ohm=rt, rbottom_H_ohm=10000,
                           output_min_screen_V=lo, output_max_screen_V=hi,
                           mux_5p5_margin_V=5.5 - hi,
                           tps62811_vin_min_V=2.75, feedback_bounds_conditional=True,
                           status='NO_GUARANTEED_LOW_VIN_REGULATION_OR_TRANSIENT_BOUND'))
    write_csv('preregulator_voltage_screen.csv', prereg)
    power = []
    for usb, intermediate, i33, eta1, eta2 in itertools.product(
            (4.5, 4.75, 5., 5.25, 5.5), (4.2, 4.5), (.1, .2, .35, .5), (.8, .85, .9), (.85, .9)):
        output = 3.3 * i33
        input_power = output / (eta1 * eta2)
        usb_current = input_power / usb
        intermediate_current = output / (eta2 * intermediate)
        # Dropout headroom screen at assumed R_HS0.6Ω and inductor0.15Ω.
        # The published0.6Ω maximum is atVIN>=6V, not USB-low; cannot use
        # it as a guaranteed bound at4.5V. These values remain explicit H.
        power.append(dict(usb_at_board_H_V=usb, intermediate_H_V=intermediate,
                          output3p3_H_A=i33, eta1_H=eta1, eta2_H=eta2,
                          usb_load_only_A=usb_current,
                          intermediate_A=intermediate_current,
                          losses_two_converters_H_W=input_power - output,
                          prereg_ron_low_vin_H_ohm=.6, inductor_dcr_H_ohm=.15,
                          efuse_ron_screen_ohm=.131, mux_ron_screen_ohm=.060,
                          required_usb_for_regulation_H_V=intermediate + intermediate_current * .75 + usb_current * .131,
                          intermediate_after_mux_H_V=intermediate - intermediate_current * .060,
                          overhead_and_inrush_not_included=True,
                          status='PARAMETRIC_ONLY'))
    write_csv('usb_cascade_power.csv', power)

    # Do not compare currents at different rails without their power path.
    # The following are exclusively screening currents referred to the12V
    # direct/AON branches. Original LM5164 max is specified at24V, hence
    # sums remain conditional, never a12V guaranteed whole-system budget.
    wake = [dict(architecture='A0_LM74900_sleep', controller_uA=10., can_uA=30.,
                 buck_shutdown_uA=15., extra_screen_uA=0.,
                 subtotal_uA=55., residual_to_100_uA=45.,
                 status='FAULT_WAKE_DEADLOCK_OPEN'),
            dict(architecture='LM74910H_sleep_auto_retry', controller_uA=10., can_uA=30.,
                 buck_shutdown_uA=15., extra_screen_uA=0.,
                 subtotal_uA=55., residual_to_100_uA=45.,
                 status='OCP_RETRY_ONLY_THERMAL_LATCH_AND_RETRY_PEAKS_OPEN'),
            dict(architecture='LM74721_always_on_protected', controller_uA=35., can_uA=30.,
                 buck_shutdown_uA=15., extra_screen_uA=0.,
                 subtotal_uA=80., residual_to_100_uA=20.,
                 status='NEW_OCP_FUSE_SOA_AND_LEAKAGE_DESIGN_REQUIRED'),
            dict(architecture='LM74900_plus_independent_LDO_timer', controller_uA=10., can_uA=30.,
                 buck_shutdown_uA=15., extra_screen_uA=15.05,
                 subtotal_uA=70.05, residual_to_100_uA=29.95,
                 status='INCOMPLETE_BRANCH_PROTECTION_DETECTOR_AND_PULSE_LOGIC')]
    write_csv('wake_architecture_screen.csv', wake)
    manifest = {str(p.relative_to(OUT)):dict(bytes=p.stat().st_size,
                sha256=hashlib.sha256(p.read_bytes()).hexdigest())
                for p in sorted((OUT / 'sources').glob('*.pdf'))}
    summary = dict(status='STUDY_ONLY_HR01_HR02_NOT_CLOSED',
                   ovp_candidates=len(devices), ovp_cases=len(ovp),
                   charge_cases=len(charge), inrush_cases=len(inrush),
                   power_cases=len(power), wake_cases=len(wake),
                   no_a0_schematic_changes=True, no_pcb=True,
                   unresolved=['Maximum OVP response/overshoot', 'CC/default USB and ROM current',
                               'Preregulator low-VIN dropout/transient proof',
                               'Partial-power/reverse current measurements',
                               'Complete protected wake budget and fault recovery'])
    (OUT / 'analysis_summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    (OUT / 'source_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
