#ifndef AG_NIMBLE_TRANSPORT_H
#define AG_NIMBLE_TRANSPORT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "ag_admission.h"
/* One singleton, NimBLE host task only. All hooks mandatory and trusted, never
 * populated from wire JSON. caller owns boot journal, admission and endpoint.
 * Callback context has static/program lifetime, including failed startup.
 * No callback may reenter start/close/send; tick may call send once.
 * Any fault is latched until reboot; no stop/restart API is implemented. */
typedef enum { AG_NIMBLE_FAULT_STORE=1, AG_NIMBLE_FAULT_HOST_RESET,
    AG_NIMBLE_FAULT_TERMINATE, AG_NIMBLE_FAULT_START, AG_NIMBLE_FAULT_TIMER } ag_nimble_fault_t;
typedef struct {
    void *context;
    void (*fault)(void *,ag_nimble_fault_t);
    bool (*advertise_allowed)(void *);
    bool (*connect)(void *,uint16_t handle,uint64_t generation,const uint8_t client[16]);
    void (*disconnect)(void *,uint64_t generation);
    /* Must authorize the local admission attempt AND display the code before
     * returning it. Six digits, transient; never log or persist the value. */
    bool (*passkey_display)(void *,uint64_t generation,char display[7]);
    void (*clear_display)(void *,uint64_t generation);
    bool (*security_changed)(void *,uint64_t generation);
    bool (*authorize_private)(void *,uint64_t generation);
    bool (*receive)(void *,uint64_t generation,const uint8_t *,size_t,uint64_t now_ms);
    bool (*tick)(void *,uint64_t generation,uint64_t now_ms);
    void (*transmitted)(void *,uint64_t generation,bool confirmed);
} ag_nimble_callbacks_t;
/* NVS must already be initialized, journal restored, qualified callbacks ready.
 * This function DOES start the radio if invoked. Compile-study app_main never
 * invokes it. No automatic erase, bond eviction, key logging or restart. */
esp_err_t ag_nimble_transport_start(const ag_nimble_callbacks_t *);
ag_bond_lookup_t ag_nimble_lookup_bond(uint64_t generation);
bool ag_nimble_security(uint64_t generation,ag_link_security_t *);
/* Host task only, checks qualified awake/admission policy before advertising. */
bool ag_nimble_advertise(void);
/* Same owner task only, one bounded frame. Confirmation is every ATT indication
 * ACK (BLE_HS_EDONE), not just stack enqueue/status0. Guard checked per chunk. */
bool ag_nimble_send(uint64_t generation,const uint8_t *,size_t);
void ag_nimble_close(uint64_t generation);
#endif
