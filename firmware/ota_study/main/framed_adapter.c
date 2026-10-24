#include "framed_adapter.h"
#include <string.h>
static bool allowed(void *context,uint64_t lease){
    ag_ota_framed_adapter_t *s=context;
    return s->payload.platform.maintenance_allowed(s->payload.platform.context,lease);
}
static uint64_t clock_ms(void *context){
    ag_ota_framed_adapter_t *s=context;
    return s->payload.platform.clock_ms(s->payload.platform.context);
}
bool ag_ota_framed_adapter_init(ag_ota_framed_adapter_t *s,const ag_manifest_trust_t *trust,
    const ag_ota_transfer_platform_t *platform,uint64_t lease,uint32_t frame_timeout_ms,uint32_t request_timeout_ms){
    if(s==NULL||trust==NULL||platform==NULL||s->initialized||s->payload.initialized
        ||s->endpoint.initialized||lease==0||frame_timeout_ms==0||request_timeout_ms==0)return false;
    ag_manifest_trust_t trust_copy=*trust;ag_ota_transfer_platform_t platform_copy=*platform;
    memset(s,0,sizeof(*s));
    if(!ag_ota_transfer_adapter_init(&s->payload,&trust_copy,&platform_copy))return false;
    ag_ot_ep_hooks_t hooks={s,allowed,clock_ms,frame_timeout_ms,request_timeout_ms};
    if(!ag_ot_endpoint_init(&s->endpoint,&s->payload.transfer,lease,&hooks))return false;
    s->initialized=true;return true;
}
static ag_ot_endpoint_t *endpoint(ag_ota_framed_adapter_t *s){return s!=NULL&&s->initialized?&s->endpoint:NULL;}
ag_ot_ep_result_t ag_ota_framed_adapter_feed(ag_ota_framed_adapter_t *s,uint64_t lease,const uint8_t *bytes,size_t length){return ag_ot_endpoint_feed(endpoint(s),lease,bytes,length);}
ag_ot_ep_result_t ag_ota_framed_adapter_step(ag_ota_framed_adapter_t *s,uint64_t lease){return ag_ot_endpoint_step(endpoint(s),lease);}
ag_ot_ep_result_t ag_ota_framed_adapter_take_output(ag_ota_framed_adapter_t *s,uint64_t lease,uint8_t *bytes,size_t capacity,size_t *written){return ag_ot_endpoint_take_output(endpoint(s),lease,bytes,capacity,written);}
ag_ot_ep_result_t ag_ota_framed_adapter_poll(ag_ota_framed_adapter_t *s,uint64_t lease){return ag_ot_endpoint_poll(endpoint(s),lease);}
ag_ot_ep_result_t ag_ota_framed_adapter_close(ag_ota_framed_adapter_t *s,uint64_t lease){return ag_ot_endpoint_close(endpoint(s),lease);}
