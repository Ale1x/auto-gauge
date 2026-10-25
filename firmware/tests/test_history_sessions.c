/* Production endpoint/RPC sessions; baseline fixture supplies trusted admission,
 * config IO and history. Catalog callback is a bounded host-only test backend. */
#define main baseline_endpoint_main
#include "test_device_endpoint.c"
#undef main
static unsigned catalog_calls;
static unsigned catalog_mode;
static ag_device_history_result_t catalog(void *context,const uint8_t after[16],uint8_t limit,ag_history_sessions_page_t *page){
    fixture_t *f=context;++catalog_calls;memset(page,0,sizeof(*page));
    if(catalog_mode==1){f->secure=false;return AG_DEVICE_HISTORY_OK;}
    if(catalog_mode==2)return AG_DEVICE_HISTORY_READ_ONLY;
    if(catalog_mode==3){page->count=1;return AG_DEVICE_HISTORY_OK;} /* invalid zero SID */
    if(catalog_mode==4)return AG_DEVICE_HISTORY_UNAVAILABLE;
    unsigned eligible=0;
    for(unsigned n=1;n<=3;++n){uint8_t id[16]={0};id[0]=(uint8_t)n;
        if(after&&memcmp(id,after,16)<=0)continue;
        ++eligible;if(page->count<limit)memcpy(page->session_ids[page->count++],id,16);
    }
    page->has_more=eligible>page->count;
    if(page->has_more)memcpy(page->next_after_session_id,page->session_ids[page->count-1],16);
    return AG_DEVICE_HISTORY_OK;
}
static ag_rpc_result_t parse(const char *json,ag_rpc_request_t *request){ag_pid_mask_t supported={{0}};return ag_rpc_request_parse((const uint8_t *)json,strlen(json),&supported,request,NULL);}
static void test_strict_request(void){
    ag_rpc_request_t request;
    assert(parse("{\"op\":\"logs.sessions\",\"limit\":16}",&request)==AG_RPC_OK&&request.operation==AG_RPC_LOGS_SESSIONS&&request.limit==16&&!request.has_after_session_id);
    assert(parse("{\"limit\":1,\"after_session_id\":\"01000000000000000000000000000000\",\"op\":\"logs.sessions\"}",&request)==AG_RPC_OK&&request.has_after_session_id&&request.after_session_id[0]==1);
    const char *bad[]={"{\"op\":\"logs.sessions\"}","{\"op\":\"logs.sessions\",\"limit\":0}","{\"op\":\"logs.sessions\",\"limit\":17}",
        "{\"op\":\"logs.sessions\",\"limit\":true}","{\"op\":\"logs.sessions\",\"limit\":1.0}","{\"op\":\"logs.sessions\",\"limit\":1,\"after_session_id\":null}",
        "{\"op\":\"logs.sessions\",\"limit\":1,\"after_session_id\":\"00000000000000000000000000000000\"}",
        "{\"op\":\"logs.sessions\",\"limit\":1,\"after_session_id\":\"AB000000000000000000000000000000\"}",
        "{\"op\":\"logs.sessions\",\"limit\":1,\"after_session_id\":\"01\"}",
        "{\"op\":\"logs.sessions\",\"limit\":1,\"after_session_id\":\"01000000000000000000000000000000\",\"after_session_\\u0069d\":\"02000000000000000000000000000000\"}",
        "{\"op\":\"hello\",\"limit\":1}","{\"op\":\"logs.sessions\",\"limit\":1,\"session_id\":\"01000000000000000000000000000000\"}",
        "{\"op\":\"logs.sessions\",\"limit\":1}{}"};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i){memset(&request,0x5a,sizeof(request));ag_rpc_request_t before=request;assert(parse(bad[i],&request)!=AG_RPC_OK&&!memcmp(&request,&before,sizeof(request)));}
}
static void test_typed_reply(void){
    ag_history_sessions_page_t page={0};ag_rpc_reply_t reply={.kind=AG_RPC_REPLY_LOGS_SESSIONS,.sessions=&page,.limit=16};
    uint8_t output[4097];size_t written=0;
    assert(ag_rpc_reply_encode(&reply,output,sizeof(output),&written)==AG_RPC_OK);
    assert(!strcmp((char *)output,"{\"op\":\"logs.sessions\",\"phase\":\"sessions\",\"sessions\":[],\"has_more\":false,\"next_after_session_id\":null}"));
    for(unsigned i=0;i<16;++i)page.session_ids[i][0]=(uint8_t)(i+1);
    page.count=16;page.has_more=true;memcpy(page.next_after_session_id,page.session_ids[15],16);
    assert(ag_rpc_reply_encode(&reply,output,sizeof(output),&written)==AG_RPC_OK&&written<1000);
    size_t exact=written;assert(ag_rpc_reply_encode(&reply,output,exact+1,&written)==AG_RPC_OK&&written==exact);
    uint8_t preserved[4097];memset(output,0x5a,sizeof(output));memcpy(preserved,output,sizeof(output));
    assert(ag_rpc_reply_encode(&reply,output,exact,&written)==AG_RPC_OUTPUT_SHORT&&written==0&&!memcmp(output,preserved,sizeof(output)));
    for(unsigned variant=0;variant<8;++variant){ag_history_sessions_page_t invalid=page;ag_rpc_reply_t bad_reply=reply;bad_reply.sessions=&invalid;
        switch(variant){case 0:invalid.count=17;break;case 1:memset(invalid.session_ids[0],0,16);break;case 2:memcpy(invalid.session_ids[1],invalid.session_ids[0],16);break;
            case 3:invalid.session_ids[0][0]=17;break;case 4:invalid.next_after_session_id[0]=15;break;case 5:invalid.count=0;break;
            case 6:bad_reply.has_after_session_id=true;memcpy(bad_reply.after_session_id,invalid.session_ids[0],16);break;case 7:invalid.has_more=false;break;}
        assert(ag_rpc_reply_encode(&bad_reply,output,sizeof(output),&written)!=AG_RPC_OK&&written==0&&!memcmp(output,preserved,sizeof(output)));
    }
}
static void test_endpoint_pagination_and_auth(void){
    fixture_t f;char json[4097];catalog_calls=catalog_mode=0;init(&f);
    send(&f,1,"{\"op\":\"logs.sessions\",\"limit\":2}");step(&f);output(&f,json);assert(strstr(json,"not_implemented")&&catalog_calls==0);
    f.endpoint.options.history_sessions=catalog;
    send(&f,2,"{\"op\":\"logs.sessions\",\"limit\":2}");step(&f);output(&f,json);
    assert(!strcmp(json,"{\"op\":\"logs.sessions\",\"phase\":\"sessions\",\"sessions\":[\"01000000000000000000000000000000\",\"02000000000000000000000000000000\"],\"has_more\":true,\"next_after_session_id\":\"02000000000000000000000000000000\"}"));
    send(&f,3,"{\"op\":\"logs.sessions\",\"limit\":2,\"after_session_id\":\"02000000000000000000000000000000\"}");step(&f);output(&f,json);
    assert(strstr(json,"03000000000000000000000000000000")&&strstr(json,"\"has_more\":false")&&strstr(json,"\"next_after_session_id\":null"));
    send(&f,4,"{\"op\":\"logs.sessions\",\"limit\":2,\"after_session_id\":\"03000000000000000000000000000000\"}");step(&f);output(&f,json);assert(strstr(json,"\"sessions\":[]"));
    assert(catalog_calls==3&&f.store.active.revision==1&&f.history.count==4);
    ag_connection_lease_t stale=f.lease;connect(&f,2);uint8_t wire[100];size_t n=0;const char *body="{\"op\":\"logs.sessions\",\"limit\":2}";
    assert(ag_frame_encode(1,1,(const uint8_t *)body,strlen(body),wire,sizeof(wire),&n)==AG_FRAME_OK);
    assert(ag_device_endpoint_feed(&f.endpoint,&stale,wire,n,0)==AG_DEVICE_STALE&&catalog_calls==3);
    catalog_mode=1;send(&f,1,body);assert(ag_device_endpoint_step(&f.endpoint,0)==AG_DEVICE_AUTH_REQUIRED&&catalog_calls==4&&f.endpoint.output_length==0);
}
static void test_endpoint_errors_and_output_deadline(void){
    const char *body="{\"op\":\"logs.sessions\",\"limit\":2}";
    for(unsigned mode=2;mode<=4;++mode){fixture_t f;char json[4097];init(&f);catalog_mode=mode;f.endpoint.options.history_sessions=catalog;send(&f,1,body);
        ag_device_result_t result=ag_device_endpoint_step(&f.endpoint,0);
        if(mode==3){assert(result==AG_DEVICE_BACKEND_ERROR&&!f.endpoint.attached);continue;}
        assert(result==AG_DEVICE_OK);output(&f,json);assert(strstr(json,mode==2?"history_not_operational":"history_unavailable"));
    }
    fixture_t f;init(&f);catalog_mode=0;f.endpoint.options.history_sessions=catalog;send(&f,1,body);step(&f);
    uint8_t out[4112];size_t written=99;assert(ag_device_endpoint_take_output(&f.endpoint,&f.lease,30,out,sizeof(out),&written)==AG_DEVICE_PROTOCOL_ERROR&&written==0);
}
int main(void){assert(baseline_endpoint_main()==0);test_strict_request();test_typed_reply();test_endpoint_pagination_and_auth();test_endpoint_errors_and_output_deadline();puts("PASS: 4 history-session catalog groups +5 endpoint baseline; trusted host backend only");return 0;}
