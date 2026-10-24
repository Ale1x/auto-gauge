#include "ag_diagnostics.h"
#include <math.h>
#include <string.h>

void ag_measurement_unavailable(ag_measurement_t *sample, ag_sample_status_t status) {
    if (sample == NULL) return;
    sample->status = status;
    sample->has_value = false;
    sample->value = NAN;
    sample->raw_length = 0;
    memset(sample->raw, 0, sizeof(sample->raw));
    sample->negative_response_code = 0;
}

ag_obd_result_t ag_obd_parse_measurement(const uint8_t *payload, size_t length,
                                        uint8_t expected_pid, uint32_t ecu_source,
                                        uint64_t now_ms, ag_measurement_t *sample) {
    if (sample == NULL) return AG_OBD_MALFORMED;
    memset(sample, 0, sizeof(*sample));
    sample->pid = expected_pid;
    sample->ecu_source = ecu_source;
    sample->updated_ms = now_ms;
    ag_measurement_unavailable(sample, AG_SAMPLE_ERROR);
    if (payload == NULL || length < 2) return AG_OBD_MALFORMED;
    if (payload[0] == 0x7f) {
        if (length != 3 || payload[1] != 1) return AG_OBD_MALFORMED;
        sample->negative_response_code = payload[2];
        return AG_OBD_NEGATIVE_RESPONSE;
    }
    if (payload[0] != 0x41) return AG_OBD_WRONG_SERVICE;
    if (payload[1] != expected_pid) return AG_OBD_WRONG_PID;
    if (!ag_obd_pid_implemented(expected_pid)) {
        sample->status = AG_SAMPLE_UNSUPPORTED;
        return AG_OBD_UNSUPPORTED_PID;
    }
    const size_t data_length = (expected_pid == 0x0c || expected_pid == 0x10) ? 2 : 1;
    if (length != data_length + 2) return AG_OBD_MALFORMED;
    const unsigned a = payload[2];
    const unsigned ab = data_length == 2 ? (a * 256u + payload[3]) : a;
    switch (expected_pid) {
        case 0x04: case 0x11:
            sample->value = (double)a * 100.0 / 255.0; sample->unit = AG_UNIT_PERCENT; break;
        case 0x05: case 0x0f:
            sample->value = (double)a - 40.0; sample->unit = AG_UNIT_CELSIUS; break;
        case 0x0c:
            sample->value = (double)ab / 4.0; sample->unit = AG_UNIT_RPM; break;
        case 0x0d:
            sample->value = (double)a; sample->unit = AG_UNIT_KM_PER_HOUR; break;
        case 0x10:
            sample->value = (double)ab / 100.0; sample->unit = AG_UNIT_GRAMS_PER_SECOND; break;
        default: return AG_OBD_UNSUPPORTED_PID;
    }
    memcpy(sample->raw, payload + 2, data_length);
    sample->raw_length = (uint8_t)data_length;
    sample->status = AG_SAMPLE_VALID;
    sample->has_value = true;
    return AG_OBD_OK;
}

void ag_obd_capabilities_init(ag_obd_capabilities_t *caps, uint32_t ecu_source) {
    if (caps == NULL) return;
    memset(caps, 0, sizeof(*caps));
    caps->ecu_source = ecu_source;
}

bool ag_obd_capabilities_apply(ag_obd_capabilities_t *caps, uint32_t ecu_source,
                               const uint8_t *payload, size_t length) {
    if (caps == NULL || payload == NULL || length != 6 || payload[0] != 0x41
        || payload[1] % 32u != 0 || caps->ecu_source != ecu_source) return false;
    const uint8_t page = payload[1];
    const uint32_t bitmap = ((uint32_t)payload[2] << 24) | ((uint32_t)payload[3] << 16)
                          | ((uint32_t)payload[4] << 8) | payload[5];
    ag_pid_mask_t candidate = caps->advertised;
    for (unsigned offset = 1; offset <= 32; ++offset) {
        const unsigned pid = page + offset;
        if (pid > 255) continue; /* No silent wrap of the final page's bit. */
        candidate.words[pid / 32u] &= ~(UINT32_C(1) << (pid % 32u));
        if ((bitmap & (UINT32_C(1) << (32u - offset))) != 0)
            ag_pid_mask_set(&candidate, (uint8_t)pid);
    }
    caps->advertised = candidate;
    caps->pages_seen |= (uint8_t)(1u << (page / 32u));
    return true;
}
void ag_obd_usable_pids(const ag_obd_capabilities_t *caps, ag_pid_mask_t *usable) {
    if (usable == NULL) return;
    ag_pid_mask_clear(usable);
    if (caps == NULL) return;
    for (unsigned pid = 1; pid <= 255; ++pid)
        if (ag_obd_pid_implemented((uint8_t)pid) && ag_pid_mask_has(&caps->advertised, (uint8_t)pid))
            ag_pid_mask_set(usable, (uint8_t)pid);
}
