#include "ag_frame.h"
#include <string.h>

static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void write32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8u * i));
}
static uint32_t crc32(const uint8_t *p, size_t n) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
    }
    return crc ^ UINT32_MAX;
}
static int valid_kind(uint8_t kind, uint32_t id) {
    return kind >= 1 && kind <= 3 && ((id == 0) == (kind == 3));
}
ag_frame_result_t ag_frame_decode(const uint8_t *packet, size_t size, ag_frame_view_t *view) {
    if (view == NULL) return AG_FRAME_INVALID;
    memset(view, 0, sizeof(*view));
    if (packet == NULL || size < 16 || size > AG_FRAME_MAX_SIZE) return AG_FRAME_INVALID;
    const size_t length = (size_t)packet[8] | (size_t)packet[9] << 8;
    if (packet[0] != 'A' || packet[1] != 'G' || packet[2] != 1
        || packet[10] != 0 || packet[11] != 0 || !valid_kind(packet[3], read32(packet + 4))
        || length > AG_FRAME_MAX_PAYLOAD || size != length + 16) return AG_FRAME_INVALID;
    if (crc32(packet, size - 4) != read32(packet + size - 4)) return AG_FRAME_INVALID;
    view->kind = packet[3]; view->request_id = read32(packet + 4);
    view->payload = packet + 12; view->payload_length = length;
    return AG_FRAME_OK;
}
ag_frame_result_t ag_frame_encode(uint8_t kind, uint32_t request_id,
    const uint8_t *payload, size_t length, uint8_t *output, size_t capacity, size_t *written) {
    if (written == NULL) return AG_FRAME_INVALID;
    *written = 0;
    if (output == NULL || (length != 0 && payload == NULL) || length > AG_FRAME_MAX_PAYLOAD
        || !valid_kind(kind, request_id)) return AG_FRAME_INVALID;
    if (capacity < length + 16) return AG_FRAME_NO_SPACE;
    output[0] = 'A'; output[1] = 'G'; output[2] = 1; output[3] = kind;
    write32(output + 4, request_id); output[8] = (uint8_t)length; output[9] = (uint8_t)(length >> 8);
    output[10] = 0; output[11] = 0;
    if (length != 0) memcpy(output + 12, payload, length);
    write32(output + length + 12, crc32(output, length + 12));
    *written = length + 16;
    return AG_FRAME_OK;
}
