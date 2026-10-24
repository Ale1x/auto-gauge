#include "ag_core.h"
#include <string.h>

bool ag_session_clock_init(ag_session_clock_t *clock,
                           const uint8_t session_id[AG_SESSION_ID_BYTES], uint64_t boot_us) {
    if (clock == NULL) return false;
    memset(clock, 0, sizeof(*clock));
    if (session_id == NULL) return false;
    uint8_t any = 0;
    for (size_t i = 0; i < AG_SESSION_ID_BYTES; ++i) any |= session_id[i];
    if (any == 0) return false;
    memcpy(clock->session_id, session_id, AG_SESSION_ID_BYTES);
    clock->boot_us = boot_us;
    clock->last_us = boot_us;
    clock->valid = true;
    return true;
}
bool ag_session_clock_stamp(ag_session_clock_t *clock, uint64_t now_us,
                            uint64_t *monotonic_ms) {
    if (clock == NULL || monotonic_ms == NULL || !clock->valid) return false;
    if (now_us < clock->last_us) {
        clock->valid = false;
        return false;
    }
    clock->last_us = now_us;
    *monotonic_ms = (now_us - clock->boot_us) / 1000;
    return true;
}
