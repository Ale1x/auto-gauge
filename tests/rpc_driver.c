/* Host-only typed RPC codec harness; NOT an RPC dispatcher. */
#include "ag_rpc.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpc_history_fixtures.h"
static ag_pid_mask_t all(void){ag_pid_mask_t m;for(size_t i=0;i<AG_PID_MASK_WORDS;++i)m.words[i]=UINT32_MAX;return m;}
static ag_rpc_result_t checked(const uint8_t *in,size_t n,const ag_pid_mask_t *m,ag_rpc_request_t *out){
    memset(out,0xa5,sizeof(*out));ag_rpc_request_t before;memcpy(&before,out,sizeof(before));
    ag_rpc_error_t error;
    ag_rpc_result_t r=ag_rpc_request_parse(in,n,m,out,&error);
    assert(error.offset<=n);
    if(r!=AG_RPC_OK)assert(memcmp(out,&before,sizeof(before))==0);
    return r;
}
static void session(const uint8_t *id){for(size_t i=0;i<16;++i)printf("%02x",id[i]);}
static ag_rpc_result_t encoded(const ag_rpc_reply_t *reply,uint8_t *out,size_t cap,size_t *written){
    memset(out,0xa5,AG_RPC_MAX_PAYLOAD+1);uint8_t before[AG_RPC_MAX_PAYLOAD+1];memcpy(before,out,sizeof(before));
    uint8_t history_before[sizeof(ag_history_snapshot_t)];
    const bool has_history=reply->kind==AG_RPC_REPLY_LOGS_READ&&reply->history!=NULL;
    if(has_history)memcpy(history_before,reply->history,sizeof(history_before));
    *written=123;
    ag_rpc_result_t result=ag_rpc_reply_encode(reply,out,cap,written);
    if(has_history)assert(memcmp(history_before,reply->history,sizeof(history_before))==0);
    if(result!=AG_RPC_OK){assert(*written==0);assert(memcmp(out,before,sizeof(before))==0);}
    else {assert(*written<=AG_RPC_MAX_PAYLOAD&&*written<cap&&out[*written]==0);}
    return result;
}
int main(int argc,char **argv){
    uint8_t input[AG_RPC_MAX_PAYLOAD+1],output[AG_RPC_MAX_PAYLOAD+1];
    size_t length=fread(input,1,sizeof(input),stdin),written;
    if(ferror(stdin)||argc<2)return 3;
    ag_pid_mask_t mask=all();if(argc>2&&strcmp(argv[2],"none")==0)ag_pid_mask_clear(&mask);
    ag_rpc_request_t request;
    if(strcmp(argv[1],"stress")==0){
        assert(checked(input,length,&mask,&request)==AG_RPC_OK);
        for(size_t n=0;n<length;++n)assert(checked(input,n,&mask,&request)!=AG_RPC_OK);
        for(size_t i=0;i<length;++i){uint8_t old=input[i];input[i]=0;assert(checked(input,length,&mask,&request)!=AG_RPC_OK);input[i]=0xff;assert(checked(input,length,&mask,&request)!=AG_RPC_OK);input[i]=old;}
        uint32_t state=11;
        for(unsigned trial=0;trial<1024;++trial){state=state*UINT32_C(1664525)+UINT32_C(1013904223);size_t n=state%sizeof(input);for(size_t i=0;i<n;++i){state=state*UINT32_C(1664525)+UINT32_C(1013904223);input[i]=(uint8_t)(state>>24);}(void)checked(input,n,&mask,&request);}
        return 0;
    }
    if(strcmp(argv[1],"selftest")==0){
        memset(&request,0xa5,sizeof(request));ag_rpc_request_t before;memcpy(&before,&request,sizeof(before));
        assert(ag_rpc_request_parse(NULL,0,&mask,&request,NULL)==AG_RPC_ARGUMENT);
        assert(ag_rpc_request_parse(input,0,NULL,&request,NULL)==AG_RPC_ARGUMENT);
        assert(ag_rpc_request_parse(input,0,&mask,NULL,NULL)==AG_RPC_ARGUMENT);
        assert(memcmp(&before,&request,sizeof(before))==0);
        ag_rpc_reply_t reply={.kind=AG_RPC_REPLY_LOGS_READ};
        reply.limit=1;
        assert(encoded(&reply,output,sizeof(output),&written)==AG_RPC_HISTORY_INVALID);
        reply.history=&kRPCSnapshots[0];memcpy(reply.session_id,reply.history->session_id,16);
        reply.limit=0;assert(encoded(&reply,output,sizeof(output),&written)==AG_RPC_ARGUMENT);
        reply.limit=17;assert(encoded(&reply,output,sizeof(output),&written)==AG_RPC_ARGUMENT);
        reply.limit=1;reply.after_sequence=7;
        assert(encoded(&reply,output,sizeof(output),&written)==AG_RPC_CURSOR_AHEAD);
        reply.after_sequence=0;reply.session_id[0]^=1;
        assert(encoded(&reply,output,sizeof(output),&written)==AG_RPC_SESSION_MISMATCH);
        reply.kind=AG_RPC_REPLY_APPLIED;reply.applied_revision=2;
        assert(encoded(&reply,output,sizeof(output),&written)==AG_RPC_ARGUMENT);
        reply.kind=AG_RPC_REPLY_LOGS_ACK;memset(reply.session_id,0,sizeof(reply.session_id));
        assert(encoded(&reply,output,sizeof(output),&written)==AG_RPC_ARGUMENT);
        reply.kind=AG_RPC_REPLY_REJECTED;reply.error=(ag_rpc_wire_error_t)999;
        assert(encoded(&reply,output,sizeof(output),&written)==AG_RPC_ARGUMENT);
        reply.kind=(ag_rpc_reply_kind_t)-1;
        assert(encoded(&reply,output,sizeof(output),&written)==AG_RPC_UNSUPPORTED);
        reply.kind=AG_RPC_REPLY_HELLO;reply.supported=&mask;reply.device_id="bad\n";
        assert(encoded(&reply,output,sizeof(output),&written)==AG_RPC_STRING_SUBSET);
        reply.device_id="ok";assert(encoded(&reply,output,1,&written)==AG_RPC_OUTPUT_SHORT);
        assert(ag_rpc_reply_encode(&reply,NULL,0,&written)==AG_RPC_ARGUMENT&&written==0);
        return 0;
    }
    if(strcmp(argv[1],"parse")==0){
        ag_rpc_result_t r=checked(input,length,&mask,&request);if(r!=AG_RPC_OK){fprintf(stderr,"result=%u\n",(unsigned)r);return 2;}
        printf("{\"op\":\"%s\",\"expected_revision\":%" PRId32 ",\"sequence\":\"%" PRIu64 "\",\"session_id\":\"",ag_rpc_operation_name(request.operation),request.expected_revision,request.sequence);
        session(request.session_id);printf("\",\"limit\":%u",request.limit);
        if(request.operation==AG_RPC_CONFIG_VALIDATE||request.operation==AG_RPC_CONFIG_APPLY){
            ag_rpc_reply_t reply={.kind=AG_RPC_REPLY_CURRENT,.simulation=true,.configuration=&request.configuration,.persistence=AG_RPC_VOLATILE};
            assert(encoded(&reply,output,sizeof(output),&written)==AG_RPC_OK);printf(",\"configuration_response\":%s",output);
        }
        puts("}");return 0;
    }
    if(strcmp(argv[1],"reply")!=0||argc<3)return 3;
    ag_config_t configuration;
    ag_rpc_reply_t reply={.simulation=true,.persistence=AG_RPC_VOLATILE};
    const char *kind=argv[2];
    const char *capacity_mode=argc>3?argv[3]:NULL;
    ag_history_snapshot_t invalid_history;
    if(strcmp(kind,"hello")==0){
        reply.kind=AG_RPC_REPLY_HELLO;reply.device_id="SIMULATO-001";
        ag_pid_mask_clear(&mask);const uint8_t pids[]={4,5,12,13,15,16,17};for(size_t i=0;i<sizeof(pids);++i)ag_pid_mask_set(&mask,pids[i]);reply.supported=&mask;
    }else if(strcmp(kind,"current")==0){
        if(ag_config_json_parse(input,length,&mask,&configuration,NULL)!=AG_CONFIG_JSON_OK)return 2;
        reply.kind=AG_RPC_REPLY_CURRENT;reply.configuration=&configuration;
    }else if(strcmp(kind,"validated")==0)reply.kind=AG_RPC_REPLY_VALIDATED;
    else if(strcmp(kind,"received")==0)reply.kind=AG_RPC_REPLY_RECEIVED;
    else if(strcmp(kind,"applied")==0){reply.kind=AG_RPC_REPLY_APPLIED;reply.applied_revision=2;}
    else if(strcmp(kind,"durable")==0){reply.kind=AG_RPC_REPLY_APPLIED;reply.applied_revision=2;reply.simulation=false;reply.persistence=AG_RPC_DURABLE;}
    else if(strcmp(kind,"revision_conflict")==0){reply.kind=AG_RPC_REPLY_REJECTED;reply.error=AG_RPC_ERROR_REVISION_CONFLICT;}
    else if(strcmp(kind,"session_mismatch")==0){reply.kind=AG_RPC_REPLY_REJECTED;reply.error=AG_RPC_ERROR_SESSION_MISMATCH;}
    else if(strcmp(kind,"read")==0){
        if(checked(input,length,&mask,&request)!=AG_RPC_OK||request.operation!=AG_RPC_LOGS_READ)return 2;
        const char *name=argc>3?argv[3]:"golden";
        capacity_mode=argc>4?argv[4]:NULL;
        reply.kind=AG_RPC_REPLY_LOGS_READ;
        memcpy(reply.session_id,request.session_id,16);reply.after_sequence=request.sequence;reply.limit=request.limit;
        if(strcmp(name,"invalid")==0){invalid_history=kRPCSnapshots[0];invalid_history.capacity=0;reply.history=&invalid_history;}
        else if(strcmp(name,"null")==0)reply.history=NULL;
        else {
            size_t index=0;
            while(index<sizeof(kRPCSnapshots)/sizeof(kRPCSnapshots[0])&&strcmp(kRPCSnapshotNames[index],name)!=0)++index;
            if(index>=sizeof(kRPCSnapshots)/sizeof(kRPCSnapshots[0]))return 3;
            reply.history=&kRPCSnapshots[index];
        }
    }
    else if(strcmp(kind,"ack")==0){
        if(checked(input,length,&mask,&request)!=AG_RPC_OK||request.operation!=AG_RPC_LOGS_ACK)return 2;
        reply.kind=AG_RPC_REPLY_LOGS_ACK;memcpy(reply.session_id,request.session_id,16);reply.acknowledged_through=request.sequence;
    }else return 3;
    if(capacity_mode!=NULL&&strcmp(capacity_mode,"real")==0)reply.simulation=false;
    size_t capacity=sizeof(output);
    if(capacity_mode!=NULL&&strcmp(capacity_mode,"short")==0)capacity=1;
    if(capacity_mode!=NULL&&strcmp(capacity_mode,"exact")==0){assert(encoded(&reply,output,sizeof(output),&written)==AG_RPC_OK);capacity=written;}
    ag_rpc_result_t result=encoded(&reply,output,capacity,&written);
    if(result!=AG_RPC_OK)return 2;
    puts((const char *)output);return 0;
}
