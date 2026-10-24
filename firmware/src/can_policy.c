#include "ag_core.h"
#include <string.h>

void ag_can_policy_init(ag_can_policy_t *policy) {
    if (policy != NULL) memset(policy, 0, sizeof(*policy));
}
bool ag_can_tx_allowed(const ag_can_policy_t *policy) {
    return policy != NULL && policy->hardware_validated && policy->rails_valid
        && (policy->power == AG_POWER_OBD || policy->power == AG_POWER_OBD_AND_USB)
        && policy->ignition == AG_IGN_QUALIFIED_ON && !policy->standby_requested
        && !policy->bus_off && policy->diagnostic_session_active;
}

bool ag_obd_pid_implemented(uint8_t pid) {
    /* Initial explicit measurement allowlist, not the full Service01 catalog. */
    switch (pid) {
        case 0x04: case 0x05: case 0x0c: case 0x0d: case 0x0f: case 0x10: case 0x11: return true;
        default: return false;
    }
}

bool ag_obd_make_read_request(const ag_can_policy_t *policy,
                              const ag_pid_mask_t *supported,
                              ag_read_kind_t kind, uint8_t pid,
                              ag_obd_request_t *request) {
    if (request == NULL) return false;
    memset(request, 0, sizeof(*request));
    if (!ag_can_tx_allowed(policy)) return false;
    if (kind == AG_READ_DISCOVERY) {
        if (pid % 32u != 0) return false;
    } else if (kind == AG_READ_MEASUREMENT) {
        if (!ag_obd_pid_implemented(pid) || !ag_pid_mask_has(supported, pid)) return false;
    } else return false;
    request->bytes[0] = 0x01;
    request->bytes[1] = pid;
    request->length = 2;
    return true;
}
