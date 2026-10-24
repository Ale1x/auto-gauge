#include "ag_config_json.h"
#include <limits.h>
#include <string.h>

typedef struct {
    const uint8_t *data;
    size_t length, position, tokens;
    unsigned depth;
    ag_config_json_result_t result;
    ag_config_result_t validation;
} parser_t;
typedef enum { ROOT, BRIGHTNESS, DASHBOARD, WIDGET, RECT } object_kind_t;
static bool fail(parser_t *p, ag_config_json_result_t result) {
    if (p->result == AG_CONFIG_JSON_OK) p->result = result;
    return false;
}
static bool whitespace(uint8_t c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static void skip_space(parser_t *p) {
    while (p->position < p->length && whitespace(p->data[p->position])) ++p->position;
}
static bool consume(parser_t *p, uint8_t c) {
    skip_space(p);
    if (p->position >= p->length || p->data[p->position] != c) return fail(p, AG_CONFIG_JSON_SYNTAX);
    ++p->position;
    return true;
}
static bool token(parser_t *p) {
    if (p->tokens >= AG_CONFIG_JSON_MAX_TOKENS) return fail(p, AG_CONFIG_JSON_TOKENS);
    ++p->tokens;
    return true;
}
static bool begin(parser_t *p, uint8_t c) {
    skip_space(p);
    if (p->position >= p->length || p->data[p->position] != c) return fail(p, AG_CONFIG_JSON_TYPE);
    if (p->depth >= AG_CONFIG_JSON_MAX_DEPTH) return fail(p, AG_CONFIG_JSON_DEPTH);
    if (!token(p)) return false;
    ++p->position; ++p->depth;
    return true;
}
static int hex(uint8_t c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static bool string(parser_t *p, char out[AG_CONFIG_JSON_MAX_STRING + 1]) {
    skip_space(p);
    if (p->position >= p->length || p->data[p->position] != '"') return fail(p, AG_CONFIG_JSON_TYPE);
    if (!token(p)) return false;
    ++p->position;
    size_t used = 0;
    while (p->position < p->length) {
        unsigned c = p->data[p->position++];
        if (c == '"') { out[used] = 0; return true; }
        if (c == '\\') {
            if (p->position >= p->length) return fail(p, AG_CONFIG_JSON_SYNTAX);
            c = p->data[p->position++];
            if (c == 'u') {
                if (p->length - p->position < 4) return fail(p, AG_CONFIG_JSON_SYNTAX);
                c = 0;
                for (unsigned i = 0; i < 4; ++i) {
                    const int digit = hex(p->data[p->position++]);
                    if (digit < 0) return fail(p, AG_CONFIG_JSON_SYNTAX);
                    c = c * 16u + (unsigned)digit;
                }
            } else if (c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't') {
                return fail(p, AG_CONFIG_JSON_STRING_SUBSET);
            } else if (c != '"' && c != '\\' && c != '/') return fail(p, AG_CONFIG_JSON_SYNTAX);
        }
        if (c < 0x20 || c > 0x7e) return fail(p, AG_CONFIG_JSON_STRING_SUBSET);
        if (used >= AG_CONFIG_JSON_MAX_STRING) return fail(p, AG_CONFIG_JSON_STRING_SUBSET);
        out[used++] = (char)c;
    }
    return fail(p, AG_CONFIG_JSON_SYNTAX);
}
static bool integer(parser_t *p, int32_t *out) {
    skip_space(p);
    if (p->position >= p->length) return fail(p, AG_CONFIG_JSON_SYNTAX);
    const bool negative = p->data[p->position] == '-';
    if (!negative && (p->data[p->position] < '0' || p->data[p->position] > '9')) return fail(p, AG_CONFIG_JSON_TYPE);
    if (!token(p)) return false;
    if (negative) ++p->position;
    if (p->position >= p->length || p->data[p->position] < '0' || p->data[p->position] > '9') return fail(p, AG_CONFIG_JSON_NUMBER);
    const bool zero = p->data[p->position] == '0';
    const uint32_t limit = negative ? UINT32_C(2147483648) : UINT32_C(2147483647);
    uint32_t value = 0;
    size_t digits = 0;
    while (p->position < p->length && p->data[p->position] >= '0' && p->data[p->position] <= '9') {
        const unsigned d = p->data[p->position] - '0';
        if ((zero && digits != 0) || value > (limit - d) / 10u) return fail(p, AG_CONFIG_JSON_NUMBER);
        value = value * 10u + d; ++digits; ++p->position;
    }
    if (p->position < p->length) {
        const uint8_t c = p->data[p->position];
        if (!whitespace(c) && c != ',' && c != ']' && c != '}') return fail(p, AG_CONFIG_JSON_NUMBER);
    }
    *out = negative ? (int32_t)(-(int64_t)value) : (int32_t)value;
    return true;
}
/* 1 = next entry, 0 = container ended, -1 = error. No trailing commas. */
static int next(parser_t *p, bool *first, uint8_t close) {
    skip_space(p);
    if (p->position < p->length && p->data[p->position] == close) {
        ++p->position; --p->depth; return 0;
    }
    if (!*first) {
        if (!consume(p, ',')) return -1;
        skip_space(p);
        if (p->position >= p->length || p->data[p->position] == close) { fail(p, AG_CONFIG_JSON_SYNTAX); return -1; }
    }
    *first = false;
    return 1;
}
static bool object(parser_t *, object_kind_t, void *);
static bool dashboards(parser_t *p, ag_config_t *config) {
    if (!begin(p, '[')) return false;
    bool first = true; int state;
    while ((state = next(p, &first, ']')) == 1) {
        if (config->dashboard_count >= AG_MAX_DASHBOARDS) return fail(p, AG_CONFIG_JSON_ARRAY_LIMIT);
        if (!object(p, DASHBOARD, &config->dashboards[config->dashboard_count])) return false;
        ++config->dashboard_count;
    }
    return state == 0;
}
static bool widgets(parser_t *p, ag_dashboard_t *dashboard) {
    if (!begin(p, '[')) return false;
    bool first = true; int state;
    while ((state = next(p, &first, ']')) == 1) {
        if (dashboard->widget_count >= AG_MAX_WIDGETS) return fail(p, AG_CONFIG_JSON_ARRAY_LIMIT);
        if (!object(p, WIDGET, &dashboard->widgets[dashboard->widget_count])) return false;
        ++dashboard->widget_count;
    }
    return state == 0;
}
static bool value(parser_t *p, object_kind_t kind, unsigned key, void *target) {
    int32_t n;
    if (kind == ROOT) {
        ag_config_t *c = target;
        if (key == 2) return object(p, BRIGHTNESS, c);
        if (key == 3) return dashboards(p, c);
        if (!integer(p, &n)) return false;
        if (key == 0) c->schema_version = (uint32_t)n; else c->revision = n;
    } else if (kind == BRIGHTNESS) {
        ag_config_t *c = target;
        if (key == 0) {
            char mode[AG_CONFIG_JSON_MAX_STRING + 1];
            if (!string(p, mode)) return false;
            if (strcmp(mode, "manual") == 0) c->brightness_mode = AG_BRIGHTNESS_MANUAL;
            else if (strcmp(mode, "auto") == 0) c->brightness_mode = AG_BRIGHTNESS_AUTO;
            else { p->validation = AG_CONFIG_BRIGHTNESS; return fail(p, AG_CONFIG_JSON_CONFIG_INVALID); }
        } else { if (!integer(p, &n)) return false; c->manual_percent = n; }
    } else if (kind == DASHBOARD) {
        ag_dashboard_t *d = target;
        if (key == 1) return widgets(p, d);
        if (!integer(p, &n)) return false;
        d->id = n;
    } else if (kind == WIDGET) {
        ag_widget_t *w = target;
        if (key == 2) return object(p, RECT, &w->rect);
        if (!integer(p, &n)) return false;
        if (key == 0) w->id = n; else w->pid = n;
    } else {
        ag_rect_t *r = target;
        if (!integer(p, &n)) return false;
        switch (key) { case 0: r->x = n; break; case 1: r->y = n; break; case 2: r->width = n; break; default: r->height = n; break; }
    }
    return true;
}
static bool object(parser_t *p, object_kind_t kind, void *target) {
    static const char *const keys[5][4] = {
        {"schema_version", "revision", "brightness", "dashboards"},
        {"mode", "manual_percent", NULL, NULL}, {"id", "widgets", NULL, NULL},
        {"id", "pid", "rect", NULL}, {"x", "y", "width", "height"}
    };
    static const unsigned counts[5] = {4, 2, 2, 3, 4};
    if (!begin(p, '{')) return false;
    unsigned seen = 0; bool first = true; int state;
    while ((state = next(p, &first, '}')) == 1) {
        char key[AG_CONFIG_JSON_MAX_STRING + 1];
        if (!string(p, key)) return false;
        unsigned index = 0;
        while (index < counts[kind] && strcmp(key, keys[kind][index]) != 0) ++index;
        if (index == counts[kind]) return fail(p, AG_CONFIG_JSON_UNKNOWN_KEY);
        if (seen & (1u << index)) return fail(p, AG_CONFIG_JSON_DUPLICATE_KEY);
        seen |= 1u << index;
        if (!consume(p, ':') || !value(p, kind, index, target)) return false;
    }
    if (state < 0) return false;
    if (seen != (1u << counts[kind]) - 1u) return fail(p, AG_CONFIG_JSON_MISSING_KEY);
    return true;
}
ag_config_json_result_t ag_config_json_parse(const uint8_t *input, size_t length,
    const ag_pid_mask_t *supported, ag_config_t *output, ag_config_json_error_t *error) {
    if (error != NULL) *error = (ag_config_json_error_t){0, AG_CONFIG_OK};
    if (input == NULL || supported == NULL || output == NULL) return AG_CONFIG_JSON_ARGUMENT;
    if (length > AG_CONFIG_JSON_MAX_BYTES) return AG_CONFIG_JSON_SIZE;
    parser_t p = { .data = input, .length = length, .result = AG_CONFIG_JSON_OK };
    ag_config_t candidate;
    memset(&candidate, 0, sizeof(candidate));
    if (object(&p, ROOT, &candidate)) {
        skip_space(&p);
        if (p.position != length) fail(&p, AG_CONFIG_JSON_SYNTAX);
        else {
            const ag_config_result_t valid = ag_config_validate(&candidate, supported);
            if (valid != AG_CONFIG_OK) {
                fail(&p, AG_CONFIG_JSON_CONFIG_INVALID);
                p.validation = valid;
            }
        }
    }
    if (error != NULL) { error->offset = p.position; error->validation = p.validation; }
    if (p.result == AG_CONFIG_JSON_OK) *output = candidate;
    return p.result;
}
