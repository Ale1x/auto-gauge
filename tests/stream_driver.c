#include "ag_stream.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    ag_stream_t *stream;
    size_t calls, payload_bytes;
    bool reject, reenter;
    uint32_t last_id;
} sink_t;
static bool accept(void *context,const ag_frame_view_t *frame) {
    sink_t *sink=context;
    ++sink->calls;sink->payload_bytes+=frame->payload_length;sink->last_id=frame->request_id;
    if(sink->reenter) {
        size_t unused=0;const uint8_t byte=0;
        assert(ag_stream_feed(sink->stream,&byte,1,0,accept,sink,&unused)==AG_STREAM_BUSY);
        assert(ag_stream_tick(sink->stream,0)==AG_STREAM_BUSY);
        assert(!ag_stream_reset(sink->stream));
    }
    return !sink->reject;
}
static size_t frame(uint8_t *output,size_t capacity,size_t payload_length) {
    uint8_t payload[AG_FRAME_MAX_PAYLOAD];
    for(size_t i=0;i<payload_length;++i) payload[i]=(uint8_t)i;
    size_t length=0;
    assert(ag_frame_encode(1,7,payload,payload_length,output,capacity,&length)==AG_FRAME_OK);
    return length;
}
static void split_and_many(void) {
    uint8_t packet[64];const size_t length=frame(packet,sizeof(packet),3);
    for(size_t split=0;split<=length;++split) {
        ag_stream_t s;sink_t sink={.stream=&s};size_t accepted=0;
        assert(ag_stream_init(&s,10000));
        if(split!=0) assert(ag_stream_feed(&s,packet,split,0,accept,&sink,&accepted)==AG_STREAM_OK);
        if(split!=length) assert(ag_stream_feed(&s,packet+split,length-split,1,accept,&sink,&accepted)==AG_STREAM_OK);
        assert(sink.calls==1&&sink.payload_bytes==3&&sink.last_id==7&&s.used==0);
    }
    ag_stream_t s;sink_t sink={.stream=&s};size_t accepted=0;
    assert(ag_stream_init(&s,10000));
    memcpy(packet+length,packet,length);
    assert(ag_stream_feed(&s,packet,length*2,0,accept,&sink,&accepted)==AG_STREAM_OK);
    assert(accepted==2&&sink.calls==2&&s.used==0);
}
static void maximum_and_deadline(void) {
    uint8_t packet[AG_FRAME_MAX_SIZE];const size_t length=frame(packet,sizeof(packet),4096);
    ag_stream_t s;sink_t sink={.stream=&s};size_t accepted;
    assert(ag_stream_init(&s,10000));
    for(size_t pos=0;pos<length;pos+=20) {
        size_t n=length-pos;if(n>20)n=20;
        assert(ag_stream_feed(&s,packet+pos,n,pos,accept,&sink,&accepted)==AG_STREAM_OK);
        assert(s.used<=AG_FRAME_MAX_SIZE);
    }
    assert(sink.calls==1&&sink.payload_bytes==4096);
    assert(ag_stream_feed(&s,packet,8,5000,accept,&sink,&accepted)==AG_STREAM_OK);
    assert(ag_stream_feed(&s,packet+8,4,14999,accept,&sink,&accepted)==AG_STREAM_OK);
    assert(ag_stream_tick(&s,15000)==AG_STREAM_TIMEOUT&&s.used==0);
    assert(ag_stream_feed(&s,packet,16,15001,accept,&sink,&accepted)==AG_STREAM_OK);
    assert(ag_stream_reset(&s)&&s.used==0);
}
static void clock_fault(void) {
    uint8_t packet[32];const size_t length=frame(packet,sizeof(packet),0);
    ag_stream_t s;sink_t sink={.stream=&s};size_t accepted;
    assert(ag_stream_init(&s,10));
    assert(ag_stream_feed(&s,packet,8,UINT64_MAX-5,accept,&sink,&accepted)==AG_STREAM_OK);
    assert(ag_stream_tick(&s,UINT64_MAX)==AG_STREAM_OK);
    assert(ag_stream_feed(&s,packet+8,length-8,UINT64_MAX,accept,&sink,&accepted)==AG_STREAM_OK);
    assert(sink.calls==1);
    assert(ag_stream_tick(&s,0)==AG_STREAM_CLOCK_FAULT);
    assert(ag_stream_reset(&s));
    assert(ag_stream_feed(&s,packet,length,UINT64_MAX,accept,&sink,&accepted)==AG_STREAM_CLOCK_FAULT);
    assert(ag_stream_init(&s,10));
    assert(ag_stream_feed(&s,packet,length,0,accept,&sink,&accepted)==AG_STREAM_OK);
}
static void invalid_input_and_headers(void) {
    uint8_t packet[513];const size_t length=frame(packet,sizeof(packet),3);
    ag_stream_t s;sink_t sink={.stream=&s};size_t accepted;
    assert(!ag_stream_init(&s,0));assert(ag_stream_tick(&s,0)==AG_STREAM_ARGUMENT);
    assert(ag_stream_init(&s,10));
    assert(ag_stream_feed(&s,packet,1,0,accept,&sink,&accepted)==AG_STREAM_OK);
    assert(ag_stream_feed(&s,packet,513,1,accept,&sink,&accepted)==AG_STREAM_INVALID_CHUNK&&s.used==0);
    assert(ag_stream_feed(&s,packet,0,2,accept,&sink,&accepted)==AG_STREAM_INVALID_CHUNK);
    assert(ag_stream_feed(&s,NULL,1,3,accept,&sink,&accepted)==AG_STREAM_ARGUMENT);
    for(size_t index=0;index<12;++index) {
        uint8_t bad[32];memcpy(bad,packet,length);
        if(index>=4&&index<=7) memset(bad+4,0,4);
        else if(index==8||index==9) {bad[8]=1;bad[9]=16;}
        else bad[index]^=0xff;
        assert(ag_stream_feed(&s,bad,12,4+index,accept,&sink,&accepted)==AG_STREAM_INVALID_FRAME);
        assert(s.used==0&&accepted==0);
    }
}
static void crc_no_resync_and_callback(void) {
    uint8_t packet[64];const size_t length=frame(packet,sizeof(packet),3);
    ag_stream_t s;sink_t sink={.stream=&s};size_t accepted;
    assert(ag_stream_init(&s,10));
    memcpy(packet+length,packet,length);packet[length*2-1]^=1;
    assert(ag_stream_feed(&s,packet,length*2,0,accept,&sink,&accepted)==AG_STREAM_INVALID_FRAME);
    assert(accepted==1&&sink.calls==1&&s.used==0); /* no rollback of first callback */
    sink.reject=true;
    assert(ag_stream_feed(&s,packet,length,1,accept,&sink,&accepted)==AG_STREAM_HANDLER_REJECTED);
    assert(accepted==0&&sink.calls==2&&s.used==0);
    sink.reject=false;sink.reenter=true;
    assert(ag_stream_feed(&s,packet,length,2,accept,&sink,&accepted)==AG_STREAM_OK);
    assert(accepted==1&&s.used==0);
}
static void random_bounded_inputs(void) {
    ag_stream_t s;sink_t sink={.stream=&s};size_t accepted;
    uint32_t state=123;uint8_t input[512];
    for(unsigned trial=0;trial<2048;++trial) {
        assert(ag_stream_init(&s,10));
        for(size_t i=0;i<sizeof(input);++i){state=state*UINT32_C(1664525)+UINT32_C(1013904223);input[i]=(uint8_t)(state>>24);}
        size_t count=1+state%512;
        (void)ag_stream_feed(&s,input,count,trial,accept,&sink,&accepted);
        assert(s.used<=AG_FRAME_MAX_SIZE);
    }
}
int main(void) {
    split_and_many();maximum_and_deadline();clock_fault();invalid_input_and_headers();
    crc_no_resync_and_callback();random_bounded_inputs();
    puts("PASS: 6 stream groups; no radio, RPC authorization or target execution");
    return 0;
}
