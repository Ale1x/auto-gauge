#include "ag_diagnostics.h"
#include <string.h>

bool ag_can_profile_valid(const ag_can_profile_t *profile) {
    if (profile == NULL || (profile->id_format != AG_CAN_ID_11 && profile->id_format != AG_CAN_ID_29)
        || (profile->bitrate != 250000 && profile->bitrate != 500000)) return false;
    const uint32_t max_id = profile->id_format == AG_CAN_ID_11 ? 0x7ff : 0x1fffffff;
    return profile->request_id <= max_id && profile->response_id <= max_id
        && profile->request_id != profile->response_id;
}

static void clear_transaction(ag_isotp_rx_t *rx) {
    rx->active = false;
    rx->complete = false;
    rx->expected_length = rx->received_length = 0;
    rx->next_sequence = 0;
    rx->fc_pending = rx->fc_prepared = rx->fc_overflow = rx->has_cf = false;
    rx->fc_count = 0;
    memset(rx->payload, 0, sizeof(rx->payload));
}
bool ag_isotp_init(ag_isotp_rx_t *rx, const ag_can_profile_t *profile, uint32_t timeout_ms) {
    if (rx == NULL) return false;
    const bool valid = ag_can_profile_valid(profile) && timeout_ms > 0;
    const ag_can_profile_t copy = valid ? *profile : (ag_can_profile_t){0};
    memset(rx, 0, sizeof(*rx));
    if (!valid) return false;
    rx->profile = copy;
    rx->timeout_ms = timeout_ms;
    rx->initialized = true;
    return true;
}
void ag_isotp_reset(ag_isotp_rx_t *rx) {
    if (rx == NULL) return;
    clear_transaction(rx);
    rx->has_time = false;
}
ag_isotp_result_t ag_isotp_poll(ag_isotp_rx_t *rx, uint64_t now_ms) {
    if (rx == NULL || !rx->initialized) return AG_ISOTP_NOT_INITIALIZED;
    if (rx->has_time && now_ms < rx->last_observed_ms) {
        clear_transaction(rx);
        return AG_ISOTP_CLOCK_REGRESSION;
    }
    rx->last_observed_ms = now_ms;
    rx->has_time = true;
    if ((rx->fc_pending && now_ms - rx->fc_requested_ms >= rx->timeout_ms)
        || (rx->active && !rx->fc_pending && now_ms - rx->last_frame_ms >= rx->timeout_ms)) {
        clear_transaction(rx);
        return AG_ISOTP_TIMEOUT;
    }
    return rx->fc_pending ? AG_ISOTP_FLOW_CONTROL_REQUIRED
        : rx->complete ? AG_ISOTP_COMPLETE : rx->active ? AG_ISOTP_WAITING : AG_ISOTP_IDLE;
}

ag_isotp_result_t ag_isotp_feed(ag_isotp_rx_t *rx, const ag_can_frame_t *frame, uint64_t now_ms) {
    const ag_isotp_result_t time = ag_isotp_poll(rx, now_ms);
    if (time == AG_ISOTP_NOT_INITIALIZED || time == AG_ISOTP_CLOCK_REGRESSION || time == AG_ISOTP_TIMEOUT) return time;
    if (frame == NULL) return AG_ISOTP_MALFORMED;
    if (frame->id != rx->profile.response_id || frame->extended != (rx->profile.id_format == AG_CAN_ID_29))
        return AG_ISOTP_IGNORED;
    if (rx->complete) return AG_ISOTP_BUSY;
    if (rx->fc_pending) { clear_transaction(rx); return AG_ISOTP_MALFORMED; }
    if (frame->rtr || frame->fd) { clear_transaction(rx); return AG_ISOTP_UNSUPPORTED; }
    if (frame->length < 1 || frame->length > 8) { clear_transaction(rx); return AG_ISOTP_MALFORMED; }
    const uint8_t type = frame->data[0] >> 4;
    if (type == 0 || type == 1) {
        if (rx->active) { clear_transaction(rx); return AG_ISOTP_MALFORMED; }
        if (type == 0) {
            const size_t length = frame->data[0] & 0x0f;
            if (length == 0 || length > 7 || length > (size_t)frame->length - 1)
                return AG_ISOTP_MALFORMED;
            memcpy(rx->payload, frame->data + 1, length);
            rx->expected_length = rx->received_length = length;
            rx->complete = true;
            return AG_ISOTP_COMPLETE;
        }
        if (frame->length != 8) return AG_ISOTP_MALFORMED;
        const size_t length = ((size_t)(frame->data[0] & 0x0f) << 8) | frame->data[1];
        if (length == 0) return AG_ISOTP_UNSUPPORTED; /* No extended 32-bit FF length. */
        if (length <= 7) return AG_ISOTP_MALFORMED;
        if (rx->fc_enabled && frame->data[2] != 0x41
            && !(frame->data[2] == 0x7f && frame->data[3] == 0x01))
            return AG_ISOTP_UNSUPPORTED;
        if (length > AG_ISOTP_CAPACITY) {
            if (rx->fc_enabled) {
                rx->fc_pending = rx->fc_overflow = true;
                rx->fc_requested_ms = now_ms;
            }
            return AG_ISOTP_OVERFLOW;
        }
        memcpy(rx->payload, frame->data + 2, 6);
        rx->expected_length = length;
        rx->received_length = 6;
        rx->next_sequence = 1;
        rx->last_frame_ms = now_ms;
        rx->active = true;
        if (rx->fc_enabled) {
            rx->fc_pending = true;
            rx->fc_requested_ms = now_ms;
            return AG_ISOTP_FLOW_CONTROL_REQUIRED;
        }
        return AG_ISOTP_WAITING;
    }
    if (type != 2) { clear_transaction(rx); return AG_ISOTP_UNSUPPORTED; }
    if (!rx->active || frame->length < 2) { clear_transaction(rx); return AG_ISOTP_MALFORMED; }
    if ((frame->data[0] & 0x0f) != rx->next_sequence) {
        clear_transaction(rx); return AG_ISOTP_SEQUENCE_ERROR;
    }
    if (rx->fc_enabled && rx->has_cf && now_ms - rx->last_cf_ms < rx->fc_stmin_ms) {
        clear_transaction(rx); return AG_ISOTP_SEPARATION_ERROR;
    }
    const size_t remaining = rx->expected_length - rx->received_length;
    const size_t available = frame->length - 1;
    if (available < remaining && available != 7) { clear_transaction(rx); return AG_ISOTP_MALFORMED; }
    const size_t length = available < remaining ? available : remaining;
    memcpy(rx->payload + rx->received_length, frame->data + 1, length);
    rx->received_length += length;
    rx->next_sequence = (rx->next_sequence + 1) & 0x0f;
    rx->last_frame_ms = now_ms;
    rx->last_cf_ms = now_ms;
    rx->has_cf = true;
    if (rx->received_length == rx->expected_length) {
        rx->active = false;
        rx->complete = true;
        return AG_ISOTP_COMPLETE;
    }
    if (rx->fc_enabled && rx->fc_block_size != 0 && ++rx->fc_count == rx->fc_block_size) {
        rx->fc_pending = true;
        rx->fc_requested_ms = now_ms;
        return AG_ISOTP_FLOW_CONTROL_REQUIRED;
    }
    return AG_ISOTP_WAITING;
}

bool ag_isotp_enable_flow_control(ag_isotp_rx_t *rx, uint8_t block_size, uint8_t stmin_ms) {
    if (rx == NULL || !rx->initialized || rx->active || rx->complete || rx->fc_pending || stmin_ms > 127)
        return false;
    /* This profile must provide the physical return ID; never emit FC to the
     * standard OBD functional request addresses. No address derivation here. */
    if ((rx->profile.id_format == AG_CAN_ID_11 && rx->profile.request_id == 0x7df)
        || (rx->profile.id_format == AG_CAN_ID_29 && rx->profile.request_id == 0x18db33f1)) return false;
    rx->fc_enabled = true;
    rx->fc_block_size = block_size;
    rx->fc_stmin_ms = stmin_ms;
    return true;
}
bool ag_isotp_fc_prepare(ag_isotp_rx_t *rx, const ag_can_policy_t *policy, uint64_t now_ms) {
    if (rx == NULL || !ag_can_tx_allowed(policy)) {
        if (rx != NULL) ag_isotp_reset(rx);
        return false;
    }
    if (ag_isotp_poll(rx, now_ms) != AG_ISOTP_FLOW_CONTROL_REQUIRED) return false;
    rx->fc_prepared = true;
    return true;
}
bool ag_isotp_fc_dispatch(ag_isotp_rx_t *rx, const ag_can_policy_t *policy,
                          uint64_t now_ms, ag_can_frame_t *output) {
    if (output == NULL) return false;
    memset(output, 0, sizeof(*output));
    if (rx == NULL || !ag_can_tx_allowed(policy)) {
        if (rx != NULL) ag_isotp_reset(rx);
        return false;
    }
    if (ag_isotp_poll(rx, now_ms) != AG_ISOTP_FLOW_CONTROL_REQUIRED || !rx->fc_prepared) return false;
    output->id = rx->profile.request_id;
    output->extended = rx->profile.id_format == AG_CAN_ID_29;
    output->length = 8; /* Classical OBD padding: zero bytes after FC parameters. */
    output->data[0] = rx->fc_overflow ? 0x32 : 0x30;
    if (rx->fc_overflow) clear_transaction(rx);
    else {
        output->data[1] = rx->fc_block_size;
        output->data[2] = rx->fc_stmin_ms;
        rx->fc_pending = rx->fc_prepared = false;
        rx->fc_count = 0;
        rx->last_frame_ms = now_ms;
    }
    return true;
}
bool ag_isotp_take(ag_isotp_rx_t *rx, uint8_t *payload, size_t capacity, size_t *length) {
    if (length != NULL) *length = 0;
    if (rx == NULL || payload == NULL || length == NULL || !rx->complete || capacity < rx->received_length)
        return false;
    memcpy(payload, rx->payload, rx->received_length);
    *length = rx->received_length;
    clear_transaction(rx);
    return true;
}
