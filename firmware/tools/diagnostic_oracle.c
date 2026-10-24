/* Host-only oracle: execute the actual C decoder, never transmit diagnostic data. */
#include "ag_diagnostics.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    unsigned count=0,cases=0;
    puts("{\"schema_version\":1,\"cases\":[");
    for(unsigned pid=0;pid<256;++pid) {
        if(!ag_obd_pid_implemented((uint8_t)pid))continue;
        unsigned length=0;
        for(unsigned trial=1;trial<=4;++trial) {
            uint8_t payload[6]={0x41,(uint8_t)pid,0,0,0,0};
            ag_measurement_t sample;
            if(ag_obd_parse_measurement(payload,trial+2,(uint8_t)pid,0x7e8,0,&sample)==AG_OBD_OK) {
                assert(length==0);length=trial;
            }
        }
        assert(length==1 || length==2 || length==4);++count;
        for(unsigned vector=0;vector<4;++vector) {
            uint8_t payload[6]={0x41,(uint8_t)pid,0,0,0,0};
            for(unsigned i=0;i<length;++i)payload[i+2]=vector==2?255:(vector==3?(uint8_t)(0x12+0x22*i):0);
            if(vector==1)payload[2]=128;
            ag_measurement_t sample;
            assert(ag_obd_parse_measurement(payload,length+2,(uint8_t)pid,0x7e8,0,&sample)==AG_OBD_OK);
            assert(sample.has_value && sample.status==AG_SAMPLE_VALID && sample.raw_length==length);
            if(cases++)puts(",");
            printf("{\"pid\":%u,\"raw\":\"",pid);
            for(unsigned i=0;i<length;++i)printf("%02x",sample.raw[i]);
            printf("\",\"value\":%.17g,\"unit_code\":%u}",sample.value,(unsigned)sample.unit);
        }
    }
    printf("\n],\"pid_count\":%u}\n",count);
    assert(count==53 && cases==212);
    return 0;
}
