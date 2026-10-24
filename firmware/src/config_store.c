#include "ag_config_store.h"
#include <string.h>

const uint8_t ag_config_commit_marker[4] = {'C', 'O', 'M', '1'};
static const uint8_t magic[4] = {'A', 'G', 'C', 'F'};

static void put16(uint8_t *p, uint16_t n) { p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8); }
static void put32(uint8_t *p, uint32_t n) { for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(n >> (8u * i)); }
static void put64(uint8_t *p, uint64_t n) { for (unsigned i = 0; i < 8; ++i) p[i] = (uint8_t)(n >> (8u * i)); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8); }
static uint32_t get32(const uint8_t *p) { uint32_t n = 0; for (unsigned i = 0; i < 4; ++i) n |= (uint32_t)p[i] << (8u * i); return n; }
static uint64_t get64(const uint8_t *p) { uint64_t n = 0; for (unsigned i = 0; i < 8; ++i) n |= (uint64_t)p[i] << (8u * i); return n; }
static uint32_t crc_add(uint32_t crc, const uint8_t *p, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        crc ^= p[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0);
    }
    return crc;
}
static uint32_t record_crc(const uint8_t *record, size_t payload_length) {
    uint32_t crc = crc_add(UINT32_MAX, record, 24);
    crc = crc_add(crc, record + 28, 4);
    return crc_add(crc, record + AG_CONFIG_HEADER_BYTES, payload_length) ^ UINT32_MAX;
}

ag_config_store_result_t ag_config_record_encode(const ag_config_t *config, const ag_pid_mask_t *supported,
                                                 uint64_t generation, uint8_t *output,
                                                 size_t capacity, size_t *written) {
    if (written != NULL) *written = 0;
    if (output == NULL || written == NULL || generation == 0) return AG_CONFIG_STORE_INVALID;
    if (ag_config_validate(config, supported) != AG_CONFIG_OK) return AG_CONFIG_STORE_CONFIG_INVALID;
    uint8_t record[AG_CONFIG_RECORD_MAX] = {0};
    uint8_t *payload = record + AG_CONFIG_HEADER_BYTES;
    put32(payload, config->schema_version);
    put32(payload + 4, (uint32_t)config->revision);
    payload[8] = (uint8_t)config->brightness_mode;
    payload[9] = (uint8_t)config->manual_percent;
    payload[10] = (uint8_t)config->dashboard_count;
    size_t position = 11;
    for (size_t d = 0; d < config->dashboard_count; ++d) {
        const ag_dashboard_t *dashboard = &config->dashboards[d];
        payload[position++] = (uint8_t)dashboard->id;
        payload[position++] = (uint8_t)dashboard->widget_count;
        for (size_t w = 0; w < dashboard->widget_count; ++w) {
            const ag_widget_t *widget = &dashboard->widgets[w];
            payload[position++] = (uint8_t)widget->id;
            payload[position++] = (uint8_t)widget->pid;
            put16(payload + position, (uint16_t)widget->rect.x); position += 2;
            put16(payload + position, (uint16_t)widget->rect.y); position += 2;
            put16(payload + position, (uint16_t)widget->rect.width); position += 2;
            put16(payload + position, (uint16_t)widget->rect.height); position += 2;
        }
    }
    memcpy(record, magic, sizeof(magic));
    put16(record + 4, 1);
    put16(record + 6, (uint16_t)position);
    put64(record + 8, generation);
    put32(record + 16, config->schema_version);
    put32(record + 20, (uint32_t)config->revision);
    put32(record + 24, record_crc(record, position));
    const size_t length = position + AG_CONFIG_HEADER_BYTES;
    if (capacity < length) return AG_CONFIG_STORE_OUTPUT_SHORT;
    memcpy(output, record, length);
    *written = length;
    return AG_CONFIG_STORE_OK;
}

static bool decode_payload(const uint8_t *payload, size_t length,
                            ag_config_t *config) {
    if (length < 11 || length > AG_CONFIG_PAYLOAD_MAX) return false;
    memset(config, 0, sizeof(*config));
    config->schema_version = get32(payload);
    config->revision = get32(payload + 4);
    config->brightness_mode = (ag_brightness_mode_t)payload[8];
    config->manual_percent = payload[9];
    config->dashboard_count = payload[10];
    if (config->dashboard_count < 1 || config->dashboard_count > AG_MAX_DASHBOARDS) return false;
    size_t position = 11;
    for (size_t d = 0; d < config->dashboard_count; ++d) {
        if (length - position < 2) return false;
        ag_dashboard_t *dashboard = &config->dashboards[d];
        dashboard->id = payload[position++];
        dashboard->widget_count = payload[position++];
        if (dashboard->widget_count < 1 || dashboard->widget_count > AG_MAX_WIDGETS) return false;
        for (size_t w = 0; w < dashboard->widget_count; ++w) {
            if (length - position < 10) return false;
            ag_widget_t *widget = &dashboard->widgets[w];
            widget->id = payload[position++];
            widget->pid = payload[position++];
            widget->rect.x = get16(payload + position); position += 2;
            widget->rect.y = get16(payload + position); position += 2;
            widget->rect.width = get16(payload + position); position += 2;
            widget->rect.height = get16(payload + position); position += 2;
        }
    }
    /* Integrity/schema selection is independent of current vehicle capability. */
    ag_pid_mask_t all_pids;
    for (size_t i = 0; i < AG_PID_MASK_WORDS; ++i) all_pids.words[i] = UINT32_MAX;
    return position == length && ag_config_validate(config, &all_pids) == AG_CONFIG_OK;
}
static bool all_ff(const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; ++i) if (data[i] != 0xff) return false;
    return true;
}
typedef struct {
    bool valid, io_error;
    uint64_t generation;
    ag_config_t config;
    size_t encoded_length;
    uint8_t encoded[AG_CONFIG_RECORD_MAX];
} slot_info_t;
static void read_slot(const ag_config_store_t *store, unsigned slot, slot_info_t *info) {
    memset(info, 0, sizeof(*info));
    uint8_t bytes[AG_CONFIG_SLOT_BYTES];
    if (store->io.read(store->io.context, slot * AG_CONFIG_SLOT_BYTES, bytes, sizeof(bytes)) != sizeof(bytes)) {
        info->io_error = true;
        return;
    }
    if (memcmp(bytes + AG_CONFIG_COMMIT_OFFSET, ag_config_commit_marker, 4) != 0
        || memcmp(bytes, magic, 4) != 0 || get16(bytes + 4) != 1 || get64(bytes + 8) == 0
        || get32(bytes + 28) != 0) return;
    const size_t length = get16(bytes + 6);
    if (length > AG_CONFIG_PAYLOAD_MAX || length < 11
        || get32(bytes + 24) != record_crc(bytes, length)
        || !all_ff(bytes + AG_CONFIG_HEADER_BYTES + length,
                    AG_CONFIG_COMMIT_OFFSET - AG_CONFIG_HEADER_BYTES - length)
        || !decode_payload(bytes + AG_CONFIG_HEADER_BYTES, length, &info->config)
        || get32(bytes + 16) != info->config.schema_version
        || get32(bytes + 20) != (uint32_t)info->config.revision) return;
    info->generation = get64(bytes + 8);
    info->encoded_length = length + AG_CONFIG_HEADER_BYTES;
    memcpy(info->encoded, bytes, info->encoded_length);
    info->valid = true;
}

ag_config_store_result_t ag_config_store_open(ag_config_store_t *store,
                                              const ag_config_flash_io_t *io,
                                              const ag_pid_mask_t *supported) {
    if (store == NULL) return AG_CONFIG_STORE_INVALID;
    ag_config_flash_io_t io_copy = {0};
    ag_pid_mask_t supported_copy = {0};
    if (io != NULL) io_copy = *io;
    if (supported != NULL) supported_copy = *supported;
    memset(store, 0, sizeof(*store));
    if (io == NULL || supported == NULL || io_copy.read == NULL || io_copy.program == NULL || io_copy.erase == NULL)
        return AG_CONFIG_STORE_INVALID;
    store->io = io_copy;
    store->supported = supported_copy;
    store->initialized = true;
    return ag_config_store_restore(store);
}
ag_config_store_result_t ag_config_store_restore(ag_config_store_t *store) {
    if (store == NULL || !store->initialized) return AG_CONFIG_STORE_INVALID;
    store->ready = store->writable = store->compatible = false;
    slot_info_t slots[2];
    read_slot(store, 0, &slots[0]);
    read_slot(store, 1, &slots[1]);
    if (!slots[0].valid && !slots[1].valid)
        return slots[0].io_error || slots[1].io_error ? AG_CONFIG_STORE_IO_ERROR : AG_CONFIG_STORE_NO_VALID_CONFIG;
    unsigned chosen = slots[1].valid && (!slots[0].valid || slots[1].generation > slots[0].generation) ? 1 : 0;
    if (slots[0].valid && slots[1].valid) {
        if (slots[0].generation == slots[1].generation) {
            if (slots[0].encoded_length != slots[1].encoded_length
                || memcmp(slots[0].encoded, slots[1].encoded, slots[0].encoded_length) != 0)
                return AG_CONFIG_STORE_CONFLICT;
        } else if (slots[chosen].config.revision <= slots[1 - chosen].config.revision)
            return AG_CONFIG_STORE_CONFLICT;
    }
    store->active = slots[chosen].config;
    store->generation = slots[chosen].generation;
    store->active_slot = chosen;
    store->ready = true;
    store->writable = !slots[0].io_error && !slots[1].io_error;
    store->compatible = ag_config_validate(&store->active, &store->supported) == AG_CONFIG_OK;
    if (!store->writable) return AG_CONFIG_STORE_READ_ONLY;
    return store->compatible ? AG_CONFIG_STORE_OK : AG_CONFIG_STORE_CAPABILITY_MISMATCH;
}
bool ag_config_store_get(const ag_config_store_t *store, ag_config_t *configuration) {
    if (store == NULL || configuration == NULL || !store->ready) return false;
    *configuration = store->active;
    return true;
}
static ag_config_store_result_t io_failure(ag_config_store_t *store) {
    store->ready = store->writable = store->compatible = false;
    return AG_CONFIG_STORE_IO_ERROR;
}
static bool target_is_erased(const ag_config_store_t *store, unsigned slot) {
    uint8_t bytes[AG_CONFIG_SLOT_BYTES];
    return store->io.read(store->io.context, slot * AG_CONFIG_SLOT_BYTES, bytes, sizeof(bytes)) == sizeof(bytes)
        && all_ff(bytes, sizeof(bytes));
}
static bool target_matches_uncommitted(const ag_config_store_t *store, unsigned slot,
                                        const uint8_t *record, size_t length) {
    uint8_t bytes[AG_CONFIG_SLOT_BYTES];
    return store->io.read(store->io.context, slot * AG_CONFIG_SLOT_BYTES, bytes, sizeof(bytes)) == sizeof(bytes)
        && memcmp(bytes, record, length) == 0 && all_ff(bytes + length, sizeof(bytes) - length);
}
ag_config_store_result_t ag_config_store_commit(ag_config_store_t *store, uint32_t expected_revision,
                                                const ag_config_t *candidate) {
    if (store == NULL || !store->initialized || candidate == NULL) return AG_CONFIG_STORE_INVALID;
    const ag_config_t candidate_copy = *candidate;
    const ag_config_store_result_t restored = ag_config_store_restore(store);
    if (restored != AG_CONFIG_STORE_OK && restored != AG_CONFIG_STORE_CAPABILITY_MISMATCH) return restored;
    if ((uint64_t)expected_revision != (uint64_t)store->active.revision) return AG_CONFIG_STORE_EXPECTED_REVISION;
    if (expected_revision == INT32_MAX) return AG_CONFIG_STORE_REVISION_EXHAUSTED;
    if (store->generation == UINT64_MAX) return AG_CONFIG_STORE_GENERATION_EXHAUSTED;
    if (candidate_copy.revision != (int64_t)expected_revision + 1
        || ag_config_validate(&candidate_copy, &store->supported) != AG_CONFIG_OK)
        return AG_CONFIG_STORE_CONFIG_INVALID;
    uint8_t record[AG_CONFIG_RECORD_MAX];
    size_t length;
    const ag_config_store_result_t encoded = ag_config_record_encode(&candidate_copy, &store->supported,
        store->generation + 1, record, sizeof(record), &length);
    if (encoded != AG_CONFIG_STORE_OK) return encoded;
    const unsigned target = 1 - store->active_slot;
    const uint32_t offset = target * AG_CONFIG_SLOT_BYTES;
    if (!store->io.erase(store->io.context, offset, AG_CONFIG_SLOT_BYTES) || !target_is_erased(store, target)
        || store->io.program(store->io.context, offset, record, length) != length
        || !target_matches_uncommitted(store, target, record, length)
        || store->io.program(store->io.context, offset + AG_CONFIG_COMMIT_OFFSET,
                              ag_config_commit_marker, 4) != 4) return io_failure(store);
    slot_info_t committed;
    read_slot(store, target, &committed);
    if (!committed.valid || committed.io_error || committed.encoded_length != length
        || memcmp(committed.encoded, record, length) != 0) return io_failure(store);
    store->active = committed.config;
    store->generation = committed.generation;
    store->active_slot = target;
    store->ready = store->writable = store->compatible = true;
    return AG_CONFIG_STORE_OK;
}
