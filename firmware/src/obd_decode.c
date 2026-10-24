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

/* Scalar-only Service01 subset. Formulas and omissions: DIAGNOSTIC_COVERAGE.md.
 * Original implementation; no external decoder source incorporated.
 * raw is unsigned big-endian1/2/4bytes; value=raw*numerator/denominator+offset.
 */
typedef struct {
    uint8_t pid, length;
    uint16_t numerator, denominator;
    int16_t offset;
    ag_unit_t unit;
} scalar_decoder_t;
static const scalar_decoder_t decoders[] = {
    {0x04,1,100,255,0,AG_UNIT_PERCENT},
    {0x05,1,1,1,-40,AG_UNIT_CELSIUS},
    {0x06,1,100,128,-100,AG_UNIT_PERCENT},
    {0x07,1,100,128,-100,AG_UNIT_PERCENT},
    {0x08,1,100,128,-100,AG_UNIT_PERCENT},
    {0x09,1,100,128,-100,AG_UNIT_PERCENT},
    {0x0a,1,3,1,0,AG_UNIT_KILOPASCAL},
    {0x0b,1,1,1,0,AG_UNIT_KILOPASCAL},
    {0x0c,2,1,4,0,AG_UNIT_RPM},
    {0x0d,1,1,1,0,AG_UNIT_KM_PER_HOUR},
    {0x0e,1,1,2,-64,AG_UNIT_DEGREE},
    {0x0f,1,1,1,-40,AG_UNIT_CELSIUS},
    {0x10,2,1,100,0,AG_UNIT_GRAMS_PER_SECOND},
    {0x11,1,100,255,0,AG_UNIT_PERCENT},
    {0x1f,2,1,1,0,AG_UNIT_SECOND},
    {0x21,2,1,1,0,AG_UNIT_KILOMETER},
    {0x22,2,79,1000,0,AG_UNIT_KILOPASCAL},
    {0x23,2,10,1,0,AG_UNIT_KILOPASCAL},
    {0x2c,1,100,255,0,AG_UNIT_PERCENT},
    {0x2d,1,100,128,-100,AG_UNIT_PERCENT},
    {0x2e,1,100,255,0,AG_UNIT_PERCENT},
    {0x2f,1,100,255,0,AG_UNIT_PERCENT},
    {0x30,1,1,1,0,AG_UNIT_COUNT},
    {0x31,2,1,1,0,AG_UNIT_KILOMETER},
    {0x33,1,1,1,0,AG_UNIT_KILOPASCAL},
    {0x3c,2,1,10,-40,AG_UNIT_CELSIUS},
    {0x3d,2,1,10,-40,AG_UNIT_CELSIUS},
    {0x3e,2,1,10,-40,AG_UNIT_CELSIUS},
    {0x3f,2,1,10,-40,AG_UNIT_CELSIUS},
    {0x42,2,1,1000,0,AG_UNIT_VOLT},
    {0x43,2,100,255,0,AG_UNIT_PERCENT},
    {0x44,2,1,32768,0,AG_UNIT_RATIO},
    {0x45,1,100,255,0,AG_UNIT_PERCENT},
    {0x46,1,1,1,-40,AG_UNIT_CELSIUS},
    {0x47,1,100,255,0,AG_UNIT_PERCENT},
    {0x48,1,100,255,0,AG_UNIT_PERCENT},
    {0x49,1,100,255,0,AG_UNIT_PERCENT},
    {0x4a,1,100,255,0,AG_UNIT_PERCENT},
    {0x4b,1,100,255,0,AG_UNIT_PERCENT},
    {0x4c,1,100,255,0,AG_UNIT_PERCENT},
    {0x4d,2,1,1,0,AG_UNIT_MINUTE},
    {0x4e,2,1,1,0,AG_UNIT_MINUTE},
    {0x52,1,100,255,0,AG_UNIT_PERCENT},
    {0x59,2,10,1,0,AG_UNIT_KILOPASCAL},
    {0x5a,1,100,255,0,AG_UNIT_PERCENT},
    {0x5b,1,100,255,0,AG_UNIT_PERCENT},
    {0x5c,1,1,1,-40,AG_UNIT_CELSIUS},
    {0x5d,2,1,128,-210,AG_UNIT_DEGREE},
    {0x5e,2,1,20,0,AG_UNIT_LITERS_PER_HOUR},
    {0x61,1,1,1,-125,AG_UNIT_PERCENT},
    {0x62,1,1,1,-125,AG_UNIT_PERCENT},
    {0x63,2,1,1,0,AG_UNIT_NEWTON_METER},
    {0xa6,4,1,10,0,AG_UNIT_KILOMETER}
};
_Static_assert(AG_RAW_CAPACITY >= 4, "Service01 scalar decoders require4raw bytes");
static const scalar_decoder_t *find_decoder(uint8_t pid) {
    for(size_t i=0;i<sizeof(decoders)/sizeof(decoders[0]);++i)
        if(decoders[i].pid==pid)return &decoders[i];
    return NULL;
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
    const scalar_decoder_t *decoder=find_decoder(expected_pid);
    if (!ag_obd_pid_implemented(expected_pid) || decoder==NULL) {
        sample->status = AG_SAMPLE_UNSUPPORTED;
        return AG_OBD_UNSUPPORTED_PID;
    }
    if (length != (size_t)decoder->length + 2) return AG_OBD_MALFORMED;
    uint32_t raw=0;
    for(size_t i=0;i<decoder->length;++i)raw=(raw<<8)|payload[i+2];
    /* Convert before scaling: no integer multiply overflow or truncation. */
    sample->value=(double)raw*decoder->numerator/decoder->denominator+decoder->offset;
    sample->unit=decoder->unit;
    memcpy(sample->raw, payload + 2, decoder->length);
    sample->raw_length = decoder->length;
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
