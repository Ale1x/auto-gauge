/* Explicit host behavioral fixture. No crypto, flash, radio or boot API. */
#include "ota_framed_endpoint.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {uint32_t size,written;uint64_t now;bool begun,ready,selected;} fixture_t;
static bool allowed(void *c,uint64_t lease){(void)c;return lease==1;}
static uint64_t clock_ms(void *c){fixture_t *f=c;return f->now;}
static bool begin(void *c,uint64_t lease,const uint8_t *p,size_t n,uint32_t *size){
    fixture_t *f=c;if(lease!=1||f->begun||p==NULL||n==0)return false;
    f->begun=true;*size=f->size;return true;
}
static bool write_image(void *c,uint64_t lease,uint32_t offset,const uint8_t *p,size_t n){
    fixture_t *f=c;if(lease!=1||!f->begun||f->ready||p==NULL||offset!=f->written||n>f->size-f->written)return false;
    f->written+=(uint32_t)n;return true;
}
static bool finish(void *c,uint64_t lease){fixture_t *f=c;if(lease!=1||!f->begun||f->written!=f->size)return false;f->ready=true;return true;}
static bool select_image(void *c,uint64_t lease){fixture_t *f=c;if(lease!=1||!f->ready)return false;f->selected=true;return true;}
static bool poll(void *c,uint64_t lease){(void)c;return lease==1;}
static bool cancel(void *c,uint64_t lease){(void)c;return lease==1;}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int main(int argc,char **argv){
    bool trusted=false;uint32_t size=0;
    for(int i=1;i<argc;++i){
        if(strcmp(argv[i],"--trusted-fixture-maintenance")==0){if(trusted)return 2;trusted=true;}
        else if(strcmp(argv[i],"--image-bytes")==0&&i+1<argc){
            if(size!=0)return 2;
            const char *s=argv[++i];if(*s==0)return 2;
            for(const char *p=s;*p;++p)if(*p<'0'||*p>'9')return 2;
            errno=0;char *end=NULL;unsigned long n=strtoul(s,&end,10);
            if(errno||*end||n==0||n>AG_OTA_TRANSFER_IMAGE_MAX||n%16)return 2;
            size=(uint32_t)n;
        }else return 2;
    }
    if(!trusted||size==0){fputs("Requires --trusted-fixture-maintenance --image-bytes N; behavioral host fixture only\n",stderr);return 2;}
    fixture_t fixture={0};fixture.size=size;
    ag_ot_hooks_t hooks={&fixture,allowed,clock_ms,begin,write_image,finish,select_image,poll,cancel,30000,900000};
    ag_ot_owner_t owner={0};if(!ag_ot_owner_init(&owner,&hooks))return 2;
    ag_ot_endpoint_t endpoint={0};
    ag_ot_ep_hooks_t endpoint_hooks={&fixture,allowed,clock_ms,1000,30000};
    if(!ag_ot_endpoint_init(&endpoint,&owner,1,&endpoint_hooks))return 2;
    for(;;){
        uint8_t prefix[4];size_t got=fread(prefix,1,4,stdin);if(got==0&&feof(stdin))return 0;if(got!=4)return 2;
        uint32_t n=get32(prefix);if(n==0||n>AG_STREAM_MAX_CHUNK)return 2;
        uint8_t packet[AG_STREAM_MAX_CHUNK];if(fread(packet,1,n,stdin)!=n)return 2;
        if(ag_ot_endpoint_feed(&endpoint,1,packet,n)!=AG_OT_EP_OK)return 2;
        ag_ot_ep_result_t result=ag_ot_endpoint_step(&endpoint,1);
        if(result!=AG_OT_EP_OK&&result!=AG_OT_EP_NO_OUTPUT)return 2;
        uint8_t out[32];size_t written=0;
        result=ag_ot_endpoint_take_output(&endpoint,1,out,sizeof(out),&written);
        if(result!=AG_OT_EP_OK&&result!=AG_OT_EP_NO_OUTPUT)return 2;
        uint8_t length[4]={(uint8_t)written,0,0,0};
        if(fwrite(length,1,4,stdout)!=4||(written&&fwrite(out,1,written,stdout)!=written)||fflush(stdout)!=0)return 2;
    }
}
