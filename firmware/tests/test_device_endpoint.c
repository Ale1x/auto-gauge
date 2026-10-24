#include "ag_device_endpoint.h"
#include "ag_flash_sim.h"
#include "shared_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    ag_connection_t connection;ag_factory_reset_t reset;ag_admission_t admission;ag_connection_lease_t lease;
    ag_client_id_t client;bool known[3],secure,close_during_commit,fail_after_marker,switch_during_commit;
    ag_flash_sim_t flash;ag_config_flash_io_t memory;ag_config_store_t store;
    ag_history_source_t history;ag_device_endpoint_t endpoint;ag_pid_mask_t caps;
} fixture_t;
static bool random_word(void *v,uint32_t *out){(void)v;*out=42;return true;}
static ag_bond_lookup_t lookup(void *v,const ag_client_id_t *id){fixture_t *f=v;return f->known[id->bytes[0]]?AG_BOND_RECOGNIZED:AG_BOND_UNKNOWN;}
static bool evidence(void *v,const ag_client_id_t *id,const ag_connection_lease_t *lease,ag_link_security_t *out){
    fixture_t *f=v;if(!f->secure || memcmp(id,&f->client,sizeof(*id))!=0 || lease->counter!=f->lease.counter || memcmp(lease->boot_id,f->lease.boot_id,16)!=0)return false;
    *out=(ag_link_security_t){true,true,true,true,true,128,true};return true;
}
static bool confirm(void *v,const ag_client_id_t *id,const ag_connection_lease_t *l){fixture_t *f=v;(void)l;f->known[id->bytes[0]]=true;return true;}
static size_t read_flash(void *v,uint32_t at,uint8_t *out,size_t n){fixture_t *f=v;return f->memory.read(&f->flash,at,out,n);}
static size_t program_flash(void *v,uint32_t at,const uint8_t *b,size_t n){fixture_t *f=v;
    if(f->close_during_commit){f->close_during_commit=false;(void)ag_admission_disconnect(&f->admission,&f->lease,0);}
    if(f->switch_during_commit){
        f->switch_during_commit=false;assert(ag_admission_disconnect(&f->admission,&f->lease,0)==AG_ADMISSION_OK);
        f->client=(ag_client_id_t){{2}};
        assert(ag_admission_open_window(&f->admission,true,100)==AG_ADMISSION_OK);
        assert(ag_admission_connect(&f->admission,&f->client,100,&f->lease)==AG_ADMISSION_OK);
        char display[7];assert(ag_admission_passkey(&f->admission,&f->lease,100,display)==AG_ADMISSION_OK);
        assert(ag_admission_security_changed(&f->admission,&f->lease,100)==AG_ADMISSION_OK);
    }
    size_t written=f->memory.program(&f->flash,at,b,n);
    if(f->fail_after_marker && at%4096==AG_CONFIG_COMMIT_OFFSET && written==4){f->fail_after_marker=false;ag_flash_sim_fail_at(&f->flash,1,0);}return written;
}
static bool erase_flash(void *v,uint32_t at,size_t n){fixture_t *f=v;return f->memory.erase(&f->flash,at,n);}
static ag_device_history_result_t history(void *v,const uint8_t sid[16],uint64_t after,ag_history_snapshot_t *out){
    fixture_t *f=v;if(memcmp(sid,f->history.session_id,16)!=0)return AG_DEVICE_HISTORY_SESSION_MISMATCH;
    if(ag_history_export(&f->history,out)!=AG_HISTORY_OK)return AG_DEVICE_HISTORY_UNAVAILABLE;
    return after>(out->exhausted?UINT64_MAX:out->next_sequence-1)?AG_DEVICE_HISTORY_CURSOR_AHEAD:AG_DEVICE_HISTORY_OK;
}
static void connect_at(fixture_t *f,uint8_t peer,uint64_t now){
    if(f->endpoint.attached)assert(ag_device_endpoint_disconnect(&f->endpoint,&f->lease,now)==AG_DEVICE_OK);
    f->client=(ag_client_id_t){{0}};f->client.bytes[0]=peer;
    if(!f->known[peer])assert(ag_admission_open_window(&f->admission,true,now)==AG_ADMISSION_OK);
    assert(ag_admission_connect(&f->admission,&f->client,now,&f->lease)==AG_ADMISSION_OK);
    if(!f->known[peer]){char display[7];assert(ag_admission_passkey(&f->admission,&f->lease,now,display)==AG_ADMISSION_OK);}
    assert(ag_admission_security_changed(&f->admission,&f->lease,now)==AG_ADMISSION_OK);
    assert(ag_device_endpoint_attach(&f->endpoint,&f->lease,now)==AG_DEVICE_OK);
}
static void connect(fixture_t *f,uint8_t peer){connect_at(f,peer,0);}
static void init(fixture_t *f){
    memset(f,0,sizeof(*f));f->secure=true;
    const uint8_t pids[]={4,5,12,13,15,16,17};for(size_t n=0;n<sizeof(pids);++n)ag_pid_mask_set(&f->caps,pids[n]);
    ag_flash_sim_init(&f->flash);f->memory=ag_flash_sim_io(&f->flash);uint8_t record[199];size_t written;
    assert(ag_config_record_encode(&kSharedFixture,&f->caps,1,record,sizeof(record),&written)==AG_CONFIG_STORE_OK);
    memcpy(f->flash.bytes,record,written);memcpy(f->flash.bytes+AG_CONFIG_COMMIT_OFFSET,ag_config_commit_marker,4);
    ag_config_flash_io_t io={f,read_flash,program_flash,erase_flash};assert(ag_config_store_open(&f->store,&io,&f->caps)==AG_CONFIG_STORE_OK);
    uint8_t boot[16]={1};assert(ag_connection_init(&f->connection,boot,NULL,NULL));ag_reset_snapshot_t clean=ag_reset_clean_snapshot();
    assert(ag_factory_reset_restore(&f->reset,&f->connection,NULL,true,&clean));
    ag_admission_config_t cfg={1000,1000,1,3};ag_admission_hooks_t hooks={f,random_word,lookup,evidence,confirm};
    assert(ag_admission_init(&f->admission,&f->connection,&cfg,&hooks));
    uint8_t sid[16];memset(sid,5,16);assert(ag_history_init(&f->history,sid,4,3));const uint8_t raw[2]={0x1a,0x10};
    for(unsigned n=0;n<4;++n)assert(ag_history_append(&f->history,12,0x7e8,2000+n*1000,AG_SAMPLE_VALID,raw,2)==AG_HISTORY_OK);
    ag_device_endpoint_options_t opts={.history_context=f,.history_read=history,.device_id="SIMULATO-001",.supported=f->caps,
        .config_persistence=AG_RPC_VOLATILE,.frame_timeout_ms=10,.request_timeout_ms=30,.simulation=true};
    assert(ag_device_endpoint_init(&f->endpoint,&f->admission,&f->store,&opts));connect(f,1);
}
static void send(fixture_t *f,uint32_t id,const char *json){
    uint8_t frame[4112];size_t n=0;assert(ag_frame_encode(1,id,(const uint8_t *)json,strlen(json),frame,sizeof(frame),&n)==AG_FRAME_OK);
    for(size_t at=0;at<n;){size_t chunk=n-at<17?n-at:17;
        assert(ag_device_endpoint_feed(&f->endpoint,&f->lease,frame+at,chunk,0)==AG_DEVICE_OK);at+=chunk;}
}
static void output(fixture_t *f,char json[4097]){
    uint8_t frame[4112];size_t n=0;assert(ag_device_endpoint_take_output(&f->endpoint,&f->lease,0,frame,sizeof(frame),&n)==AG_DEVICE_OK);
    ag_frame_view_t view;assert(ag_frame_decode(frame,n,&view)==AG_FRAME_OK && view.request_id==f->endpoint.last_request_id);
    memcpy(json,view.payload,view.payload_length);json[view.payload_length]=0;
}
static void step(fixture_t *f){assert(ag_device_endpoint_step(&f->endpoint,0)==AG_DEVICE_OK);}
static void apply_json(char out[2048],int revision,int expected){
    int n=snprintf(out,2048,"{\"op\":\"config.apply\",\"expected_revision\":%d,\"configuration\":{\"schema_version\":1,\"revision\":%d,\"brightness\":{\"mode\":\"manual\",\"manual_percent\":50},\"dashboards\":[{\"id\":1,\"widgets\":[{\"id\":1,\"pid\":12,\"rect\":{\"x\":0,\"y\":0,\"width\":10000,\"height\":10000}}]}]}}",expected,revision);
    assert(n>0 && n<2048);
}
static void test_auth_busy_replay_generation_and_output(void){
    fixture_t f;init(&f);ag_connection_lease_t a=f.lease;char json[4097];
    send(&f,1,"{\"op\":\"hello\"}");
    uint8_t wire[32];size_t n;assert(ag_frame_encode(1,2,(const uint8_t *)"{\"op\":\"hello\"}",14,wire,sizeof(wire),&n)==AG_FRAME_OK);
    assert(ag_device_endpoint_feed(&f.endpoint,&a,wire,n,0)==AG_DEVICE_BUSY);step(&f);
    uint8_t tiny[1]={99};size_t written=99;assert(ag_device_endpoint_take_output(&f.endpoint,&a,0,tiny,1,&written)==AG_DEVICE_OUTPUT_SHORT && written==0 && tiny[0]==99);
    output(&f,json);assert(strstr(json,"authenticated\":true")!=NULL);
    assert(ag_device_endpoint_feed(&f.endpoint,&a,wire,n,0)==AG_DEVICE_OK);step(&f);output(&f,json);
    assert(ag_device_endpoint_feed(&f.endpoint,&a,wire,n,0)==AG_DEVICE_REPLAY && !f.endpoint.attached);
    connect(&f,2);ag_connection_lease_t b=f.lease;
    assert(ag_device_endpoint_feed(&f.endpoint,&a,wire,n,0)==AG_DEVICE_STALE && f.endpoint.attached);
    send(&f,1,"{\"op\":\"hello\"}");step(&f);f.secure=false;
    assert(ag_device_endpoint_take_output(&f.endpoint,&b,0,wire,sizeof(wire),&written)==AG_DEVICE_AUTH_REQUIRED && written==0);
    f.secure=true;connect(&f,1);assert(f.lease.counter!=a.counter);send(&f,1,"{\"op\":\"hello\"}");step(&f);output(&f,json);
}
static void test_apply_two_phase_cancel_lost_response_and_ambiguous_io(void){
    fixture_t f;init(&f);char body[2048],json[4097];apply_json(body,2,1);send(&f,1,body);step(&f);
    assert(f.store.active.revision==1 && f.endpoint.phase==AG_DEVICE_APPLY_COMMIT);
    assert(ag_device_endpoint_step(&f.endpoint,0)==AG_DEVICE_BUSY);output(&f,json);assert(strstr(json,"received")!=NULL);
    assert(ag_device_endpoint_disconnect(&f.endpoint,&f.lease,0)==AG_DEVICE_OK && f.store.active.revision==1);
    connect(&f,1);send(&f,1,body);step(&f);output(&f,json);step(&f);
    assert(f.store.active.revision==2); /* Drop final application response/ACK: durable model stays applied. */
    assert(ag_device_endpoint_disconnect(&f.endpoint,&f.lease,0)==AG_DEVICE_OK);connect(&f,1);
    send(&f,1,"{\"op\":\"config.get\"}");step(&f);output(&f,json);assert(strstr(json,"\"revision\":2")!=NULL);
    apply_json(body,3,2);send(&f,2,body);step(&f);output(&f,json);f.fail_after_marker=true;step(&f);output(&f,json);
    assert(strstr(json,"outcome_unknown")!=NULL && strstr(json,"\"phase\":\"rejected\"")==NULL);
    ag_flash_sim_reboot(&f.flash);assert(ag_config_store_restore(&f.store)==AG_CONFIG_STORE_OK && f.store.active.revision==3);
    apply_json(body,4,3);send(&f,3,body);step(&f);output(&f,json);f.close_during_commit=true;
    assert(ag_device_endpoint_step(&f.endpoint,0)==AG_DEVICE_AUTH_REQUIRED && f.endpoint.output_length==0);
    assert(ag_config_store_restore(&f.store)==AG_CONFIG_STORE_OK && f.store.active.revision==4);
}
static void test_log_ack_sessions_and_diagnostics(void){
    fixture_t f;init(&f);char before[4097],after[4097];
    const char *read="{\"op\":\"logs.read\",\"session_id\":\"05050505050505050505050505050505\",\"after_sequence\":\"0\",\"limit\":2}";
    send(&f,1,read);step(&f);output(&f,before);assert(strstr(before,"\"first\":\"1\"")!=NULL);
    send(&f,2,"{\"op\":\"logs.ack\",\"session_id\":\"05050505050505050505050505050505\",\"acknowledged_through\":\"4\"}");step(&f);output(&f,after);
    assert(strstr(after,"source_records_deleted\":false")!=NULL && f.history.count==4);
    connect(&f,2);send(&f,1,read);step(&f);output(&f,after);assert(strcmp(before,after)==0);
    uint8_t sid[16];memset(sid,6,16);assert(ag_history_init(&f.history,sid,4,1));send(&f,2,read);step(&f);output(&f,after);assert(strstr(after,"session_mismatch")!=NULL);
    f.store.supported.words[0]=0;send(&f,3,"{\"op\":\"config.get\"}");step(&f);output(&f,after);
    assert(strstr(after,"capability_mismatch")!=NULL && strstr(after,"\"phase\":\"current\"")==NULL && strstr(after,"\"persistence\":\"durable\"")==NULL);
}
static void test_partial_timeout_and_unauthorized_no_metadata(void){
    fixture_t f;init(&f);uint8_t one=0x41;ag_connection_lease_t lease=f.lease;
    assert(ag_device_endpoint_feed(&f.endpoint,&lease,&one,1,0)==AG_DEVICE_OK);
    assert(ag_device_endpoint_step(&f.endpoint,10)==AG_DEVICE_PROTOCOL_ERROR && !f.endpoint.attached);
    init(&f);send(&f,1,"{\"op\":\"hello\"}");f.secure=false;
    assert(ag_device_endpoint_step(&f.endpoint,0)==AG_DEVICE_AUTH_REQUIRED && f.endpoint.output_length==0);
    init(&f);char apply[2048];apply_json(apply,2,1);send(&f,1,apply);step(&f);
    assert(ag_device_endpoint_step(&f.endpoint,30)==AG_DEVICE_PROTOCOL_ERROR && f.store.active.revision==1 && !f.endpoint.attached);
    init(&f);assert(ag_device_endpoint_disconnect(&f.endpoint,&f.lease,0)==AG_DEVICE_OK);
    assert(ag_admission_connect(&f.admission,&f.client,0,&f.lease)==AG_ADMISSION_OK);
    assert(ag_device_endpoint_attach(&f.endpoint,&f.lease,0)==AG_DEVICE_AUTH_REQUIRED && f.endpoint.output_length==0);
    init(&f);uint8_t coalesced[128];size_t a=0,b=0;
    const char *hello="{\"op\":\"hello\"}";
    assert(ag_frame_encode(1,1,(const uint8_t *)hello,strlen(hello),coalesced,sizeof(coalesced),&a)==AG_FRAME_OK);
    assert(ag_frame_encode(1,2,(const uint8_t *)hello,strlen(hello),coalesced+a,sizeof(coalesced)-a,&b)==AG_FRAME_OK);
    assert(ag_device_endpoint_feed(&f.endpoint,&f.lease,coalesced,a+b,0)==AG_DEVICE_BUSY);
    assert(!f.endpoint.attached && f.endpoint.output_length==0 && f.store.active.revision==1);
}
static void test_stale_attach_does_not_tick_new_peer_clock(void){
    fixture_t f;init(&f);ag_connection_lease_t old=f.lease;
    connect_at(&f,2,100);ag_connection_lease_t current=f.lease;
    assert(ag_device_endpoint_attach(&f.endpoint,&old,1)==AG_DEVICE_STALE);
    assert(!f.admission.clock_fault && f.admission.last_ms==100 && f.endpoint.attached);
    uint8_t packet[4112],out[4112];size_t n=0,written=0;
    const char *request="{\"op\":\"config.get\"}";
    assert(ag_frame_encode(1,1,(const uint8_t *)request,strlen(request),packet,sizeof(packet),&n)==AG_FRAME_OK);
    assert(ag_device_endpoint_feed(&f.endpoint,&current,packet,n,100)==AG_DEVICE_OK);
    assert(ag_device_endpoint_step(&f.endpoint,100)==AG_DEVICE_OK);
    assert(ag_device_endpoint_take_output(&f.endpoint,&current,101,out,sizeof(out),&written)==AG_DEVICE_OK && written>0);
    assert(ag_connection_mutations_allowed(&f.connection,&current));
    init(&f);char apply[2048],json[4097];apply_json(apply,2,1);send(&f,1,apply);step(&f);output(&f,json);
    f.switch_during_commit=true;
    assert(ag_device_endpoint_step(&f.endpoint,0)==AG_DEVICE_AUTH_REQUIRED && f.endpoint.output_length==0);
    assert(!f.admission.clock_fault && f.admission.last_ms==100 && ag_connection_mutations_allowed(&f.connection,&f.lease));
    assert(f.store.active.revision==2);
    assert(ag_device_endpoint_attach(&f.endpoint,&f.lease,100)==AG_DEVICE_OK);
    assert(ag_device_endpoint_feed(&f.endpoint,&f.lease,packet,n,100)==AG_DEVICE_OK);
    assert(ag_device_endpoint_step(&f.endpoint,100)==AG_DEVICE_OK);
    assert(ag_device_endpoint_take_output(&f.endpoint,&f.lease,101,out,sizeof(out),&written)==AG_DEVICE_OK);
}
int main(void){(void)kSharedCases;test_auth_busy_replay_generation_and_output();test_apply_two_phase_cancel_lost_response_and_ambiguous_io();
    test_log_ack_sessions_and_diagnostics();test_partial_timeout_and_unauthorized_no_metadata();test_stale_attach_does_not_tick_new_peer_clock();
    printf("PASS: 5 device endpoint groups; sizeof=%zu; trusted fixtures only, no radio\n",sizeof(ag_device_endpoint_t));return 0;}
