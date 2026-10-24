#include "ag_rpc.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const uint8_t *data;
    size_t length, position, tokens;
    unsigned depth;
    ag_rpc_result_t result;
} reader_t;
static bool bad(reader_t *p, ag_rpc_result_t result) {
    if (p->result == AG_RPC_OK) p->result = result;
    return false;
}
static bool space(uint8_t c) { return c==' ' || c=='\t' || c=='\r' || c=='\n'; }
static void ws(reader_t *p) { while (p->position<p->length && space(p->data[p->position])) ++p->position; }
static bool punctuation(reader_t *p,uint8_t c) {
    ws(p);
    if (p->position>=p->length || p->data[p->position]!=c) return bad(p,AG_RPC_SYNTAX);
    ++p->position;return true;
}
static bool token(reader_t *p) {
    if (p->tokens>=AG_RPC_MAX_TOKENS) return bad(p,AG_RPC_TOKENS);
    ++p->tokens;return true;
}
static bool begin(reader_t *p,uint8_t c) {
    ws(p);
    if (p->position>=p->length || p->data[p->position]!=c) return bad(p,AG_RPC_TYPE);
    if (p->depth>=AG_RPC_MAX_DEPTH) return bad(p,AG_RPC_DEPTH);
    if (!token(p)) return false;
    ++p->position;++p->depth;return true;
}
static int hexadecimal(uint8_t c) {
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
}
static bool text(reader_t *p,char out[AG_RPC_MAX_STRING+1]) {
    ws(p);
    if(p->position>=p->length || p->data[p->position]!='"')return bad(p,AG_RPC_TYPE);
    if(!token(p))return false;
    ++p->position;size_t n=0;
    while(p->position<p->length) {
        unsigned c=p->data[p->position++];
        if(c=='"'){out[n]=0;return true;}
        if(c=='\\') {
            if(p->position>=p->length)return bad(p,AG_RPC_SYNTAX);
            c=p->data[p->position++];
            if(c=='u') {
                if(p->length-p->position<4)return bad(p,AG_RPC_SYNTAX);
                c=0;
                for(unsigned i=0;i<4;++i){int digit=hexadecimal(p->data[p->position++]);if(digit<0)return bad(p,AG_RPC_SYNTAX);c=c*16+(unsigned)digit;}
            } else if(c=='b'||c=='f'||c=='n'||c=='r'||c=='t')return bad(p,AG_RPC_STRING_SUBSET);
            else if(c!='"'&&c!='\\'&&c!='/')return bad(p,AG_RPC_SYNTAX);
        }
        if(c<0x20||c>0x7e||n>=AG_RPC_MAX_STRING)return bad(p,AG_RPC_STRING_SUBSET);
        out[n++]=(char)c;
    }
    return bad(p,AG_RPC_SYNTAX);
}
static bool number(reader_t *p,int32_t *out) {
    ws(p);
    if(p->position>=p->length)return bad(p,AG_RPC_SYNTAX);
    bool negative=p->data[p->position]=='-';
    if(!negative&&(p->data[p->position]<'0'||p->data[p->position]>'9'))return bad(p,AG_RPC_TYPE);
    if(!token(p))return false;
    if(negative)++p->position;
    if(p->position>=p->length||p->data[p->position]<'0'||p->data[p->position]>'9')return bad(p,AG_RPC_NUMBER);
    bool zero=p->data[p->position]=='0';uint32_t value=0;
    const uint32_t limit=negative?UINT32_C(2147483648):UINT32_C(2147483647);
    size_t digits=0;
    while(p->position<p->length&&p->data[p->position]>='0'&&p->data[p->position]<='9') {
        unsigned d=p->data[p->position]-'0';
        if((zero&&digits!=0)||value>(limit-d)/10)return bad(p,AG_RPC_NUMBER);
        value=value*10+d;++digits;++p->position;
    }
    if(p->position<p->length) {
        uint8_t c=p->data[p->position];
        if(!space(c)&&c!=','&&c!=']'&&c!='}')return bad(p,AG_RPC_NUMBER);
    }
    *out=negative?(int32_t)(-(int64_t)value):(int32_t)value;return true;
}
static int next(reader_t *p,bool *first,uint8_t close) {
    ws(p);
    if(p->position<p->length&&p->data[p->position]==close){++p->position;--p->depth;return 0;}
    if(!*first) {
        if(!punctuation(p,','))return -1;
        ws(p);
        if(p->position>=p->length||p->data[p->position]==close){bad(p,AG_RPC_SYNTAX);return -1;}
    }
    *first=false;return 1;
}
/* Scan only to bound/find a configuration span. Its own strict schema parser
 * subsequently checks every key/duplicate/type. No raw JSON is retained. */
static bool scan(reader_t *p) {
    ws(p);
    if(p->position>=p->length)return bad(p,AG_RPC_SYNTAX);
    uint8_t c=p->data[p->position];
    if(c=='{'||c=='[') {
        if(!begin(p,c))return false;
        bool first=true;int state;
        while((state=next(p,&first,c=='{'?'}':']'))==1) {
            if(c=='{'){char key[AG_RPC_MAX_STRING+1];if(!text(p,key)||!punctuation(p,':'))return false;}
            if(!scan(p))return false;
        }
        return state==0;
    }
    if(c=='"'){char value[AG_RPC_MAX_STRING+1];return text(p,value);}
    int32_t ignored;return number(p,&ignored);
}
static bool session(const char *text_value,uint8_t out[AG_SESSION_ID_BYTES]) {
    if(strlen(text_value)!=32)return false;
    uint8_t any=0;
    for(size_t i=0;i<16;++i) {
        uint8_t a=(uint8_t)text_value[2*i],b=(uint8_t)text_value[2*i+1];
        if(!((a>='0'&&a<='9')||(a>='a'&&a<='f'))||!((b>='0'&&b<='9')||(b>='a'&&b<='f')))return false;
        out[i]=(uint8_t)(hexadecimal(a)*16+hexadecimal(b));any|=out[i];
    }
    return any!=0;
}
static bool counter(const char *value,uint64_t *out) {
    size_t n=strlen(value);
    if(n==0||n>20||(n>1&&value[0]=='0'))return false;
    uint64_t result=0;
    for(size_t i=0;i<n;++i) {
        if(value[i]<'0'||value[i]>'9')return false;
        unsigned digit=(unsigned)(value[i]-'0');
        if(result>(UINT64_MAX-digit)/10)return false;
        result=result*10+digit;
    }
    *out=result;return true;
}
const char *ag_rpc_operation_name(ag_rpc_operation_t operation) {
    static const char *const names[]={"hello","config.get","config.validate","config.apply","logs.read","logs.ack"};
    return (unsigned)operation<6?names[operation]:NULL;
}
ag_rpc_result_t ag_rpc_request_parse(const uint8_t *input,size_t length,const ag_pid_mask_t *supported,
                                     ag_rpc_request_t *output,ag_rpc_error_t *error) {
    if(error!=NULL)*error=(ag_rpc_error_t){0,AG_CONFIG_JSON_OK,AG_CONFIG_OK};
    if(input==NULL||supported==NULL||output==NULL)return AG_RPC_ARGUMENT;
    if(length>AG_RPC_MAX_PAYLOAD)return AG_RPC_SIZE;
    reader_t p={.data=input,.length=length,.result=AG_RPC_OK};
    ag_rpc_request_t candidate;memset(&candidate,0,sizeof(candidate));
    const char *const keys[]={"op","configuration","expected_revision","session_id","after_sequence","limit","acknowledged_through"};
    unsigned seen=0;bool first=true;int state=0;
    size_t config_start=0,config_length=0;
    if(!begin(&p,'{'))goto done;
    while((state=next(&p,&first,'}'))==1) {
        char value[AG_RPC_MAX_STRING+1];
        if(!text(&p,value))goto done;
        unsigned k=0;while(k<7&&strcmp(value,keys[k])!=0)++k;
        if(k==7){bad(&p,AG_RPC_UNKNOWN_KEY);goto done;}
        if(seen&(1u<<k)){bad(&p,AG_RPC_DUPLICATE_KEY);goto done;}
        seen|=1u<<k;
        if(!punctuation(&p,':'))goto done;
        if(k==0) {
            if(!text(&p,value))goto done;
            unsigned op=0;while(op<6&&strcmp(value,ag_rpc_operation_name((ag_rpc_operation_t)op))!=0)++op;
            if(op==6){bad(&p,AG_RPC_UNSUPPORTED);goto done;}
            candidate.operation=(ag_rpc_operation_t)op;
        } else if(k==1) {
            ws(&p);config_start=p.position;
            if(!scan(&p))goto done;
            config_length=p.position-config_start;
        } else if(k==2||k==5) {
            int32_t n;if(!number(&p,&n))goto done;
            if(n<1||(k==5&&n>16)){bad(&p,AG_RPC_NUMBER);goto done;}
            if(k==2)candidate.expected_revision=n;else candidate.limit=(uint8_t)n;
        } else {
            if(!text(&p,value))goto done;
            if(k==3) {if(!session(value,candidate.session_id)){bad(&p,AG_RPC_TYPE);goto done;}}
            else if(!counter(value,&candidate.sequence)){bad(&p,AG_RPC_NUMBER);goto done;}
        }
    }
    if(state<0)goto done;
    ws(&p);if(p.position!=length){bad(&p,AG_RPC_SYNTAX);goto done;}
    {
        static const unsigned required[]={1,1,3,7,57,73};
        if(!(seen&1)||seen!=required[candidate.operation]){bad(&p,AG_RPC_FIELDS);goto done;}
    }
    if(config_length!=0) {
        ag_config_json_error_t detail;
        ag_config_json_result_t result=ag_config_json_parse(input+config_start,config_length,supported,&candidate.configuration,&detail);
        if(result!=AG_CONFIG_JSON_OK) {
            p.position=config_start+detail.offset;bad(&p,AG_RPC_CONFIG);
            if(error!=NULL){error->configuration_error=result;error->validation=detail.validation;}
            goto done;
        }
    }
done:
    if(error!=NULL)error->offset=p.position;
    if(p.result==AG_RPC_OK)*output=candidate;
    return p.result;
}

/* Writer stages at most4096 payload bytes, preserving caller output on failure. */
typedef struct {char bytes[AG_RPC_MAX_PAYLOAD+1];size_t used;bool valid;} writer_t;
static void append(writer_t *w,const char *format,...) {
    if(!w->valid)return;
    va_list args;va_start(args,format);
    int count=vsnprintf(w->bytes+w->used,sizeof(w->bytes)-w->used,format,args);va_end(args);
    if(count<0||(size_t)count>=sizeof(w->bytes)-w->used){w->valid=false;return;}
    w->used+=(size_t)count;
}
static bool quoted(writer_t *w,const char *text_value,size_t maximum) {
    if(text_value==NULL)return false;
    size_t n=0;while(n<=maximum&&text_value[n]!=0)++n;
    if(n==0||n>maximum)return false;
    append(w,"\"");
    for(size_t i=0;i<n;++i) {
        unsigned char c=(unsigned char)text_value[i];if(c<0x20||c>0x7e)return false;
        if(c=='"'||c=='\\')append(w,"\\%c",c);else append(w,"%c",c);
    }
    append(w,"\"");return w->valid;
}
static void encoded_session(writer_t *w,const uint8_t id[AG_SESSION_ID_BYTES]) {
    append(w,"\"");for(size_t i=0;i<AG_SESSION_ID_BYTES;++i)append(w,"%02x",id[i]);append(w,"\"");
}
static bool nonzero_session(const uint8_t id[AG_SESSION_ID_BYTES]) {
    uint8_t any=0;for(size_t i=0;i<AG_SESSION_ID_BYTES;++i)any|=id[i];return any!=0;
}
static bool config(writer_t *w,const ag_config_t *c) {
    ag_pid_mask_t all;for(size_t i=0;i<AG_PID_MASK_WORDS;++i)all.words[i]=UINT32_MAX;
    if(ag_config_validate(c,&all)!=AG_CONFIG_OK)return false;
    append(w,"{\"schema_version\":%" PRIu32 ",\"revision\":%" PRId64 ",\"brightness\":{\"mode\":\"%s\",\"manual_percent\":%" PRId32 "},\"dashboards\":[",
        c->schema_version,c->revision,c->brightness_mode==AG_BRIGHTNESS_MANUAL?"manual":"auto",c->manual_percent);
    for(size_t d=0;d<c->dashboard_count;++d) {
        const ag_dashboard_t *dash=&c->dashboards[d];append(w,"%s{\"id\":%" PRId32 ",\"widgets\":[",d?",":"",dash->id);
        for(size_t j=0;j<dash->widget_count;++j) {
            const ag_widget_t *v=&dash->widgets[j];
            append(w,"%s{\"id\":%" PRId32 ",\"pid\":%" PRId32 ",\"rect\":{\"x\":%" PRId32 ",\"y\":%" PRId32 ",\"width\":%" PRId32 ",\"height\":%" PRId32 "}}",
                j?",":"",v->id,v->pid,v->rect.x,v->rect.y,v->rect.width,v->rect.height);
        }
        append(w,"]}");
    }
    append(w,"]}");return w->valid;
}
static const char *persistence(ag_rpc_persistence_t p) {
    return p==AG_RPC_VOLATILE?"volatile":p==AG_RPC_DURABLE?"durable":NULL;
}
static ag_rpc_result_t history_page(writer_t *w,const ag_rpc_reply_t *reply) {
    if(reply->limit<1||reply->limit>AG_HISTORY_PAGE_RECORDS)return AG_RPC_ARGUMENT;
    if(!w->valid||w->used>=AG_RPC_MAX_PAYLOAD)return AG_RPC_SIZE;
    /* Page includes its own NUL. Reserve one extra byte so RPC close+NUL fit:
     * prefix + page.written + 1 closing brace <=4096 payload bytes. */
    const size_t capacity=sizeof(w->bytes)-w->used-1;
    for(size_t limit=reply->limit;limit>0;--limit) {
        ag_history_page_info_t info;
        const ag_history_result_t result=ag_history_make_page(reply->history,reply->session_id,
            reply->after_sequence,limit,w->bytes+w->used,capacity,&info);
        if(result==AG_HISTORY_OK) {
            if(info.written>=capacity)return AG_RPC_SIZE;
            w->used+=info.written;return AG_RPC_OK;
        }
        if(result==AG_HISTORY_OUTPUT_SHORT)continue;
        if(result==AG_HISTORY_SESSION_MISMATCH)return AG_RPC_SESSION_MISMATCH;
        if(result==AG_HISTORY_CURSOR_AHEAD)return AG_RPC_CURSOR_AHEAD;
        return AG_RPC_HISTORY_INVALID;
    }
    return AG_RPC_SIZE;
}
ag_rpc_result_t ag_rpc_reply_encode(const ag_rpc_reply_t *reply,uint8_t *output,size_t capacity,size_t *written) {
    if(written!=NULL)*written=0;
    if(reply==NULL||output==NULL||written==NULL)return AG_RPC_ARGUMENT;
    writer_t w={.used=0,.valid=true};append(&w,"{\"simulation\":%s,",reply->simulation?"true":"false");
    switch(reply->kind) {
    case AG_RPC_REPLY_HELLO:
        if(reply->supported==NULL)return AG_RPC_ARGUMENT;
        append(&w,"\"op\":\"hello\",\"device_id\":");
        if(!quoted(&w,reply->device_id,128))return AG_RPC_STRING_SUBSET;
        append(&w,",\"protocol_version\":1,\"supported_service01_pids\":[");
        {bool first=true;for(unsigned pid=1;pid<=255;++pid)if(ag_pid_mask_has(reply->supported,(uint8_t)pid)){append(&w,"%s%u",first?"":",",pid);first=false;}}
        append(&w,"],\"authenticated\":%s",reply->authenticated?"true":"false");break;
    case AG_RPC_REPLY_CURRENT:
        if(persistence(reply->persistence)==NULL)return AG_RPC_ARGUMENT;
        append(&w,"\"phase\":\"current\",\"configuration\":");
        if(!config(&w,reply->configuration))return AG_RPC_CONFIG;
        append(&w,",\"persistence\":\"%s\"",persistence(reply->persistence));break;
    case AG_RPC_REPLY_VALIDATED:append(&w,"\"phase\":\"validated\"");break;
    case AG_RPC_REPLY_RECEIVED:append(&w,"\"phase\":\"received\"");break;
    case AG_RPC_REPLY_APPLIED:
        if(reply->applied_revision<1||persistence(reply->persistence)==NULL)return AG_RPC_ARGUMENT;
        append(&w,"\"phase\":\"applied\",\"applied_revision\":%" PRId32 ",\"persistence\":\"%s\"",reply->applied_revision,persistence(reply->persistence));break;
    case AG_RPC_REPLY_REJECTED: {
        static const char *const errors[]={NULL,"revision_conflict","session_mismatch","history_unavailable","invalid_configuration","invalid_history_request","not_implemented","unauthorized","invalid_request","invalid_message","unsupported_pid"};
        if((unsigned)reply->error>=sizeof(errors)/sizeof(errors[0])||errors[reply->error]==NULL)return AG_RPC_ARGUMENT;
        append(&w,"\"phase\":\"rejected\",\"error\":\"%s\"",errors[reply->error]);break;}
    case AG_RPC_REPLY_LOGS_READ: {
        append(&w,"\"op\":\"logs.read\",\"page\":");
        const ag_rpc_result_t result=history_page(&w,reply);
        if(result!=AG_RPC_OK)return result;
        break;}
    case AG_RPC_REPLY_LOGS_ACK:
        if(!nonzero_session(reply->session_id))return AG_RPC_ARGUMENT;
        append(&w,"\"op\":\"logs.ack\",\"session_id\":");encoded_session(&w,reply->session_id);
        append(&w,",\"acknowledged_through\":\"%" PRIu64 "\",\"source_records_deleted\":false",reply->acknowledged_through);break;
    default:return AG_RPC_UNSUPPORTED;
    }
    append(&w,"}");
    if(!w.valid)return AG_RPC_SIZE;
    if(capacity<=w.used)return AG_RPC_OUTPUT_SHORT;
    memcpy(output,w.bytes,w.used+1);*written=w.used;return AG_RPC_OK;
}
