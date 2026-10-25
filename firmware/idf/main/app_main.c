#include "ag_application.h"
#include "ag_bench_profile.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#ifdef CONFIG_AG_NATIVE_PROFILE
#include "native_profile.h"
#endif

static const char *TAG="auto_gauge";
#define AG_APPLICATION_TASK_STACK_BYTES 32768u
#ifdef CONFIG_AG_NATIVE_PROFILE
static void native_boot_task(void *unused){
    (void)unused;
    /* Initialization may restore config using nested4KiB scratch buffers.
     * Once bridge.start creates NimBLE host, no owner state is touched here. */
    bool started=ag_native_profile_start();
    if(!started&&ag_native_profile_recovery_required()){
        /* This branch never handed ownership to NimBLE and never started CAN.
         * Consume only qualified local USB/reset events; no automatic resume,
         * provisioning, radio start or reboot follows an interrupted reset. */
        ESP_LOGW(TAG,"Recovery reset locale: attesa evento fisico e USB qualificati");
        while(ag_native_profile_recovery_required()){
            if(!ag_native_profile_recovery_tick()){
                ESP_LOGE(TAG,"Recovery locale fermata: riconciliazione esplicita necessaria");break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }else if(!started)ESP_LOGW(TAG,"Profilo nativo assente/non valido: nessuna attivazione automatica");
    vTaskDelete(NULL);
}
#endif
#ifndef CONFIG_AG_NATIVE_PROFILE
static ag_application_t application;
#ifdef CONFIG_AG_SOFTWARE_BENCH
static ag_bench_profile_t bench;
#endif
/* Config restore has nested 4KiB scratch arrays; runtime reconfiguration also
 * validates a scratch runtime. This explicit development task stack is sized
 * conservatively, not a physical stack high-water/heap qualification. */
static void owner_task(void *unused){
    (void)unused;
#ifdef CONFIG_AG_SOFTWARE_BENCH
    uint8_t sid[16];esp_fill_random(sid,sizeof(sid));
    if(!ag_bench_profile_start(&bench,&application,sid)){
        ESP_LOGE(TAG,"Impossibile inizializzare banco software volatile");vTaskDelete(NULL);return;
    }
    ESP_LOGW(TAG,"SIMULAZIONE: CAN/display/storage volatili; radio e hardware non attivati");
#else
    /* The same application owner is linked and called in the default image.
     * Missing board/storage/security/trust profile is an explicit blocked boot,
     * never invented pins, fake durable storage, radio or device credentials. */
    (void)ag_application_init(&application,NULL);
    ESP_LOGW(TAG,"Profilo scheda assente: applicazione BLOCCATA, nessun GPIO/radio/flash");
#endif
    ag_application_phase_t previous=AG_APP_BLOCKED;
    for(;;){
#ifdef CONFIG_AG_SOFTWARE_BENCH
        bench.now_ms=(uint64_t)esp_timer_get_time()/1000u;
#endif
        (void)ag_application_tick(&application);
        if(previous!=application.phase){previous=application.phase;
            ESP_LOGI(TAG,"Stato applicazione: %u",(unsigned)previous);}
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
#endif
void app_main(void){
#ifdef CONFIG_AG_NATIVE_PROFILE
    if(xTaskCreate(native_boot_task,"ag_boot",AG_APPLICATION_TASK_STACK_BYTES,NULL,5,NULL)!=pdPASS)
        ESP_LOGE(TAG,"Task bootstrap non creato: nessuna periferica attivata");
#else
    if(xTaskCreate(owner_task,"ag_owner",AG_APPLICATION_TASK_STACK_BYTES,NULL,5,NULL)!=pdPASS)
        ESP_LOGE(TAG,"Task applicativo non creato: nessuna periferica attivata");
#endif
}
