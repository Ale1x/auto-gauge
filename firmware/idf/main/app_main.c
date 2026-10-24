#include "ag_core.h"
#include "ag_hal.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "auto_gauge";

void app_main(void) {
    ag_can_policy_t policy;
    ag_session_clock_t clock;
    uint8_t session[AG_SESSION_ID_BYTES];
    /* Identifier only; not an authentication token or cryptographic key. */
    esp_fill_random(session, sizeof(session));
    const uint64_t boot_us = (uint64_t)esp_timer_get_time();
    const bool clock_ready = ag_session_clock_init(&clock, session, boot_us);
    (void)ag_hal_read_power(&policy);
    ESP_LOGW(TAG, "Base di sviluppo: CAN, display, flash log e BLE non attivati");
    if (!clock_ready || ag_can_tx_allowed(&policy)) {
        ESP_LOGE(TAG, "Stato iniziale non valido: tutte le attivita restano bloccate");
        return;
    }
    /* No TWAI driver, GPIO, panel initialization, flash write or OTA here. */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
