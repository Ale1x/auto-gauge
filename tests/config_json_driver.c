/* Host-only bounded config parser harness. No radio/storage/device operations. */
#include "ag_config_json.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static ag_pid_mask_t mask(bool available) {
    ag_pid_mask_t out;
    for (size_t i = 0; i < AG_PID_MASK_WORDS; ++i) out.words[i] = available ? UINT32_MAX : 0;
    return out;
}
static ag_config_json_result_t checked(const uint8_t *input, size_t length, const ag_pid_mask_t *supported,
                                      ag_config_t *out, ag_config_json_error_t *error) {
    memset(out, 0xa5, sizeof(*out));
    ag_config_t before;
    memcpy(&before, out, sizeof(before));
    const ag_config_json_result_t result = ag_config_json_parse(input, length, supported, out, error);
    if (result != AG_CONFIG_JSON_OK) assert(memcmp(out, &before, sizeof(before)) == 0);
    assert(error->offset <= length);
    return result;
}
static void print_config(const ag_config_t *c) {
    printf("{\"schema_version\":%" PRIu32 ",\"revision\":%" PRId64 ",\"brightness\":{\"mode\":\"%s\",\"manual_percent\":%" PRId32 "},\"dashboards\":[",
           c->schema_version, c->revision, c->brightness_mode == AG_BRIGHTNESS_MANUAL ? "manual" : "auto", c->manual_percent);
    for (size_t d = 0; d < c->dashboard_count; ++d) {
        const ag_dashboard_t *dashboard = &c->dashboards[d];
        printf("%s{\"id\":%" PRId32 ",\"widgets\":[", d ? "," : "", dashboard->id);
        for (size_t w = 0; w < dashboard->widget_count; ++w) {
            const ag_widget_t *widget = &dashboard->widgets[w];
            printf("%s{\"id\":%" PRId32 ",\"pid\":%" PRId32 ",\"rect\":{\"x\":%" PRId32 ",\"y\":%" PRId32 ",\"width\":%" PRId32 ",\"height\":%" PRId32 "}}",
                   w ? "," : "", widget->id, widget->pid, widget->rect.x, widget->rect.y, widget->rect.width, widget->rect.height);
        }
        printf("]}");
    }
    printf("]}\n");
}
int main(int argc, char **argv) {
    uint8_t input[AG_CONFIG_JSON_MAX_BYTES + 1];
    const size_t length = fread(input, 1, sizeof(input), stdin);
    if (ferror(stdin)) return 3;
    ag_config_t output;
    ag_config_json_error_t error;
    const ag_pid_mask_t supported = mask(argc < 3 || strcmp(argv[2], "none") != 0);
    if (argc >= 2 && strcmp(argv[1], "selftest") == 0) {
        memset(&output, 0xa5, sizeof(output));
        ag_config_t before; memcpy(&before, &output, sizeof(before));
        assert(ag_config_json_parse(NULL, 0, &supported, &output, &error) == AG_CONFIG_JSON_ARGUMENT);
        assert(memcmp(&output, &before, sizeof(before)) == 0);
        assert(ag_config_json_parse(input, 0, NULL, &output, NULL) == AG_CONFIG_JSON_ARGUMENT);
        assert(ag_config_json_parse(input, 0, &supported, NULL, &error) == AG_CONFIG_JSON_ARGUMENT);
        assert(ag_config_json_parse(input, 0, &supported, &output, NULL) != AG_CONFIG_JSON_OK);
        assert(checked(input, sizeof(input), &supported, &output, &error) == AG_CONFIG_JSON_SIZE);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "stress") == 0) {
        assert(length < sizeof(input));
        assert(checked(input, length, &supported, &output, &error) == AG_CONFIG_JSON_OK);
        for (size_t n = 0; n < length; ++n)
            assert(checked(input, n, &supported, &output, &error) != AG_CONFIG_JSON_OK);
        for (size_t n = 0; n < length; ++n) {
            const uint8_t original = input[n];
            input[n] = 0;
            assert(checked(input, length, &supported, &output, &error) != AG_CONFIG_JSON_OK);
            input[n] = 0xff;
            assert(checked(input, length, &supported, &output, &error) != AG_CONFIG_JSON_OK);
            input[n] = original;
        }
        uint32_t random = UINT32_C(0xab31);
        for (size_t trial = 0; trial < 1024; ++trial) {
            random = random * UINT32_C(1664525) + UINT32_C(1013904223);
            const size_t n = random % sizeof(input);
            for (size_t i = 0; i < n; ++i) {
                random = random * UINT32_C(1664525) + UINT32_C(1013904223);
                input[i] = (uint8_t)(random >> 24);
            }
            (void)checked(input, n, &supported, &output, &error);
        }
        return 0;
    }
    if (argc < 2 || strcmp(argv[1], "parse") != 0) return 3;
    const ag_config_json_result_t result = checked(input, length, &supported, &output, &error);
    if (result != AG_CONFIG_JSON_OK) {
        fprintf(stderr, "result=%u offset=%zu validation=%u\n", (unsigned)result, error.offset, (unsigned)error.validation);
        return 2;
    }
    print_config(&output);
    return 0;
}
