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
    /* Explicit scalar Service01 subset; keep aligned with decoder coverage tests.
     * A decoder does not prove ECU support or authorize a request. */
    switch (pid) {
        case 0x04: case 0x05: case 0x06: case 0x07: case 0x08: case 0x09:
        case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
        case 0x10: case 0x11: case 0x1f: case 0x21: case 0x22: case 0x23:
        case 0x2c: case 0x2d: case 0x2e: case 0x2f: case 0x30: case 0x31:
        case 0x33: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
        case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
        case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e:
        case 0x52: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e:
        case 0x61: case 0x62: case 0x63: case 0xa6: return true;
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
