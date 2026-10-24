#include "ag_core.h"
#include <string.h>

void ag_pid_mask_clear(ag_pid_mask_t *mask) {
    if (mask != NULL) memset(mask, 0, sizeof(*mask));
}
void ag_pid_mask_set(ag_pid_mask_t *mask, uint8_t pid) {
    if (mask != NULL) mask->words[pid / 32u] |= UINT32_C(1) << (pid % 32u);
}
bool ag_pid_mask_has(const ag_pid_mask_t *mask, uint8_t pid) {
    return mask != NULL && (mask->words[pid / 32u] & (UINT32_C(1) << (pid % 32u))) != 0;
}

ag_config_result_t ag_config_validate(const ag_config_t *config,
                                      const ag_pid_mask_t *supported) {
    if (config == NULL || supported == NULL) return AG_CONFIG_NULL;
    if (config->schema_version != AG_CONFIG_VERSION) return AG_CONFIG_VERSION_UNSUPPORTED;
    if (config->revision < 1 || config->revision > INT32_MAX) return AG_CONFIG_REVISION;
    if (config->dashboard_count < 1 || config->dashboard_count > AG_MAX_DASHBOARDS)
        return AG_CONFIG_DASHBOARD_COUNT;
    if ((config->brightness_mode != AG_BRIGHTNESS_MANUAL && config->brightness_mode != AG_BRIGHTNESS_AUTO)
        || config->manual_percent < 0 || config->manual_percent > 100)
        return AG_CONFIG_BRIGHTNESS;
    for (size_t d = 0; d < config->dashboard_count; ++d) {
        const ag_dashboard_t *dashboard = &config->dashboards[d];
        if (dashboard->id < 1 || dashboard->id > 255) return AG_CONFIG_ID;
        for (size_t other = 0; other < d; ++other)
            if (config->dashboards[other].id == dashboard->id) return AG_CONFIG_DUPLICATE_ID;
        if (dashboard->widget_count < 1 || dashboard->widget_count > AG_MAX_WIDGETS)
            return AG_CONFIG_WIDGET_COUNT;
        for (size_t w = 0; w < dashboard->widget_count; ++w) {
            const ag_widget_t *widget = &dashboard->widgets[w];
            if (widget->id < 1 || widget->id > 255) return AG_CONFIG_ID;
            for (size_t other = 0; other < w; ++other)
                if (dashboard->widgets[other].id == widget->id) return AG_CONFIG_DUPLICATE_ID;
            const ag_rect_t *rect = &widget->rect;
            if (rect->x < 0 || rect->y < 0 || rect->x > 10000 || rect->y > 10000
                || rect->width <= 0 || rect->height <= 0
                || rect->width > 10000 || rect->height > 10000
                || rect->x + rect->width > 10000 || rect->y + rect->height > 10000)
                return AG_CONFIG_RECTANGLE;
            if (widget->pid < 1 || widget->pid > 255
                || !ag_pid_mask_has(supported, (uint8_t)widget->pid)) return AG_CONFIG_PID_UNSUPPORTED;
        }
    }
    return AG_CONFIG_OK;
}

ag_config_result_t ag_config_apply(ag_config_t *active, const ag_config_t *candidate,
                                   const ag_pid_mask_t *supported) {
    if (active == NULL) return AG_CONFIG_NULL;
    const ag_config_result_t result = ag_config_validate(candidate, supported);
    if (result == AG_CONFIG_OK) *active = *candidate;
    return result;
}
