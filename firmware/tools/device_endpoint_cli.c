/* HOST FIXTURE ONLY. stdout is binary AG protocol; no private stderr output. */
#define _POSIX_C_SOURCE 200809L
#include "ag_device_endpoint.h"
#include "ag_flash_sim.h"
#include "shared_fixture.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    ag_flash_sim_t flash;ag_config_flash_io_t memory;int fd;
    ag_connection_t connection;ag_factory_reset_t reset;ag_admission_t admission;
    ag_connection_lease_t lease;ag_client_id_t client;
    bool known[3],io_failed,drop_applied,disconnect_on_next_program,fail_after_marker;
    const char *control;
    ag_config_store_t store;ag_history_source_t history;ag_device_endpoint_t endpoint;
} fixture_t;
static fixture_t fixture;
static uint64_t now_ms(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t)!=0)return 0;return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
static bool full_write(int fd,const uint8_t *b,size_t n){
    while(n){ssize_t k=write(fd,b,n);if(k<0 && errno==EINTR)continue;if(k<=0)return false;b+=(size_t)k;n-=(size_t)k;}return true;
}
static bool persist(fixture_t *f,uint32_t at,size_t n){
    if(f->fd<0)return true;
    const uint8_t *data=f->flash.bytes+at;
    while(n){ssize_t k=pwrite(f->fd,data,n,at);if(k<0 && errno==EINTR)continue;if(k<=0){f->io_failed=true;return false;}data+=(size_t)k;at+=(uint32_t)k;n-=(size_t)k;}
    if(fsync(f->fd)!=0){f->io_failed=true;return false;}return true;
}
static size_t read_flash(void *v,uint32_t at,uint8_t *out,size_t n){
    fixture_t *f=v;if(f->io_failed)return 0;
    if(f->fd>=0){size_t done=0;
        while(done<sizeof(f->flash.bytes)){ssize_t k=pread(f->fd,f->flash.bytes+done,sizeof(f->flash.bytes)-done,(off_t)done);
            if(k<0 && errno==EINTR)continue;if(k<=0){f->io_failed=true;return 0;}done+=(size_t)k;}
    }
    return f->memory.read(&f->flash,at,out,n);
}
static size_t program_flash(void *v,uint32_t at,const uint8_t *data,size_t n){
    fixture_t *f=v;if(f->io_failed)return 0;
    if(f->disconnect_on_next_program){f->disconnect_on_next_program=false;(void)ag_admission_disconnect(&f->admission,&f->lease,now_ms());}
    size_t written=f->memory.program(&f->flash,at,data,n);
    if(written && !persist(f,at,written))return 0;
    if(f->fail_after_marker && written==4 && at%4096==AG_CONFIG_COMMIT_OFFSET){
        f->fail_after_marker=false;ag_flash_sim_fail_at(&f->flash,1,0);
    }
    return written;
}
static bool erase_flash(void *v,uint32_t at,size_t n){
    fixture_t *f=v;if(f->io_failed)return false;
    bool erased=f->memory.erase(&f->flash,at,n);
    /* Persist partial simulated erase too, not just a successful erase. */
    if(!persist(f,at,n))return false;
    return erased;
}
static bool entropy(void *v,uint32_t *out){
    (void)v;int fd=open("/dev/urandom",O_RDONLY);if(fd<0)return false;
    uint8_t b[4];size_t n=0;while(n<4){ssize_t k=read(fd,b+n,4-n);if(k<0 && errno==EINTR)continue;if(k<=0){close(fd);return false;}n+=(size_t)k;}close(fd);
    *out=(uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;return true;
}
static ag_bond_lookup_t lookup(void *v,const ag_client_id_t *client){fixture_t *f=v;return client->bytes[0]<3&&f->known[client->bytes[0]]?AG_BOND_RECOGNIZED:AG_BOND_UNKNOWN;}
static bool security(void *v,const ag_client_id_t *client,const ag_connection_lease_t *lease,ag_link_security_t *out){
    fixture_t *f=v;
    if(memcmp(client->bytes,f->client.bytes,16)!=0 || lease->counter!=f->lease.counter || memcmp(lease->boot_id,f->lease.boot_id,16)!=0)return false;
    /* EXPLICIT TRUSTED TEST FIXTURE: no radio or negotiated security exists. */
    *out=(ag_link_security_t){true,true,true,true,true,128,true};return true;
}
static bool confirm_bond(void *v,const ag_client_id_t *client,const ag_connection_lease_t *lease){
    fixture_t *f=v;(void)lease;if(client->bytes[0]>=3)return false;f->known[client->bytes[0]]=true;return true;
}
static ag_device_history_result_t history_read(void *v,const uint8_t sid[16],uint64_t after,ag_history_snapshot_t *out){
    fixture_t *f=v;if(memcmp(sid,f->history.session_id,16)!=0)return AG_DEVICE_HISTORY_SESSION_MISMATCH;
    if(ag_history_export(&f->history,out)!=AG_HISTORY_OK)return AG_DEVICE_HISTORY_UNAVAILABLE;
    uint64_t latest=out->exhausted?UINT64_MAX:out->next_sequence-1;return after>latest?AG_DEVICE_HISTORY_CURSOR_AHEAD:AG_DEVICE_HISTORY_OK;
}
static bool connect_peer(fixture_t *f,uint8_t peer){
    if(f->endpoint.attached)(void)ag_device_endpoint_disconnect(&f->endpoint,&f->lease,now_ms());
    else if(f->admission.link_open)(void)ag_admission_disconnect(&f->admission,&f->lease,now_ms());
    f->client=(ag_client_id_t){{0}};f->client.bytes[0]=peer;
    if(!f->known[peer] && ag_admission_open_window(&f->admission,true,now_ms())!=AG_ADMISSION_OK)return false;
    if(ag_admission_connect(&f->admission,&f->client,now_ms(),&f->lease)!=AG_ADMISSION_OK)return false;
    if(!f->known[peer]){char display[7];if(ag_admission_passkey(&f->admission,&f->lease,now_ms(),display)!=AG_ADMISSION_OK)return false;memset(display,0,sizeof(display));}
    return ag_admission_security_changed(&f->admission,&f->lease,now_ms())==AG_ADMISSION_OK
        && ag_device_endpoint_attach(&f->endpoint,&f->lease,now_ms())==AG_DEVICE_OK;
}
static bool seed_config(fixture_t *f,const ag_pid_mask_t *supported){
    uint8_t bytes[AG_CONFIG_RECORD_MAX];size_t n=0;
    if(ag_config_record_encode(&kSharedFixture,supported,1,bytes,sizeof(bytes),&n)!=AG_CONFIG_STORE_OK)return false;
    memcpy(f->flash.bytes,bytes,n);memcpy(f->flash.bytes+AG_CONFIG_COMMIT_OFFSET,ag_config_commit_marker,4);
    return persist(f,0,AG_CONFIG_SLOT_BYTES);
}
static bool seed_history(fixture_t *f,uint8_t sid_byte){
    uint8_t sid[16];memset(sid,sid_byte,16);if(!ag_history_init(&f->history,sid,4,3))return false;
    const uint8_t raw[2]={0x1a,0x10};for(unsigned i=0;i<4;++i)
        if(ag_history_append(&f->history,12,0x7e8,2000+i*1000,AG_SAMPLE_VALID,raw,2)!=AG_HISTORY_OK)return false;
    return true;
}
static bool control(fixture_t *f){
    if(f->control==NULL)return true;
    int fd=open(f->control,O_RDWR);if(fd<0)return errno==ENOENT;
    char command[65]={0};ssize_t n=read(fd,command,64);if(n<0){close(fd);return false;}
    if(n==64){close(fd);return false;}if(n==0){close(fd);return true;}
    if(ftruncate(fd,0)!=0){close(fd);return false;}close(fd);
    while(n>0 && (command[n-1]=='\n'||command[n-1]=='\r'))command[--n]=0;
    if(strcmp(command,"connect-a")==0)return connect_peer(f,1);
    if(strcmp(command,"connect-b")==0)return connect_peer(f,2);
    if(strcmp(command,"disconnect")==0){if(f->endpoint.attached)(void)ag_device_endpoint_disconnect(&f->endpoint,&f->lease,now_ms());return true;}
    if(strcmp(command,"drop-applied")==0){f->drop_applied=true;return true;}
    if(strcmp(command,"fail-next-io")==0){ag_flash_sim_fail_at(&f->flash,1,0);return true;}
    if(strcmp(command,"fail-after-marker")==0){f->fail_after_marker=true;return true;}
    if(strcmp(command,"restore-io")==0){
        /* A readable page cache is not a recovered durability guarantee. */
        if(f->fd>=0 && fsync(f->fd)!=0){f->io_failed=true;return true;}
        ag_flash_sim_reboot(&f->flash);f->io_failed=false;return true;
    }
    if(strcmp(command,"disconnect-during-commit")==0){f->disconnect_on_next_program=true;return true;}
    if(strcmp(command,"new-log-session")==0)return seed_history(f,6);
    return false;
}
int main(int argc,char **argv){
    fixture_t *f=&fixture;f->fd=-1;const char *store_path=NULL;bool seed=false,trusted=false;
    for(int i=1;i<argc;++i){
        if(strcmp(argv[i],"--trusted-fixture-admission")==0)trusted=true;
        else if(strcmp(argv[i],"--seed-initial")==0)seed=true;
        else if(strcmp(argv[i],"--store-file")==0 && i+1<argc)store_path=argv[++i];
        else if(strcmp(argv[i],"--control-file")==0 && i+1<argc)f->control=argv[++i];
        else return 2;
    }
    if(!trusted)return 2;
    (void)kSharedCases;
    ag_flash_sim_init(&f->flash);f->memory=ag_flash_sim_io(&f->flash);
    if(store_path!=NULL){f->fd=open(store_path,O_RDWR|O_CREAT,0600);if(f->fd<0)return 3;
        off_t length=lseek(f->fd,0,SEEK_END);
        if(length==0){if(!seed || !persist(f,0,8192))return 3;}
        else {uint8_t loaded[8192];
            /* Fence a prior writer's failed/uncertain fsync before reporting
             * an existing file as host-durable in this new process. */
            if(length!=8192 || fsync(f->fd)!=0){f->io_failed=true;return 3;}
            if(read_flash(f,0,loaded,8192)!=8192)return 3;
            memcpy(f->flash.bytes,loaded,8192);
        }
    }
    ag_pid_mask_t supported;ag_pid_mask_clear(&supported);const uint8_t pids[]={4,5,12,13,15,16,17};
    for(size_t i=0;i<sizeof(pids);++i)ag_pid_mask_set(&supported,pids[i]);
    ag_config_flash_io_t io={f,read_flash,program_flash,erase_flash};
    ag_config_store_result_t opened=ag_config_store_open(&f->store,&io,&supported);
    if(opened==AG_CONFIG_STORE_NO_VALID_CONFIG && seed){bool blank=true;for(size_t i=0;i<8192;++i)if(f->flash.bytes[i]!=0xff)blank=false;
        if(!blank || !seed_config(f,&supported))return 3;opened=ag_config_store_restore(&f->store);}
    if(opened!=AG_CONFIG_STORE_OK && opened!=AG_CONFIG_STORE_READ_ONLY && opened!=AG_CONFIG_STORE_CAPABILITY_MISMATCH)return 3;
    uint8_t boot[16];for(unsigned i=0;i<4;++i){uint32_t word;if(!entropy(f,&word))return 3;for(unsigned n=0;n<4;++n)boot[i*4+n]=(uint8_t)(word>>(8*n));}
    if(!ag_connection_init(&f->connection,boot,NULL,NULL))return 3;ag_reset_snapshot_t clean=ag_reset_clean_snapshot();
    if(!ag_factory_reset_restore(&f->reset,&f->connection,NULL,true,&clean))return 3;
    ag_admission_config_t timing={600000,600000,1,10};ag_admission_hooks_t hooks={f,entropy,lookup,security,confirm_bond};
    if(!ag_admission_init(&f->admission,&f->connection,&timing,&hooks) || !seed_history(f,5))return 3;
    ag_device_endpoint_options_t options={.history_context=f,.history_read=history_read,.device_id="SIMULATO-001",
        .supported=supported,.config_persistence=store_path?AG_RPC_DURABLE:AG_RPC_VOLATILE,
        .frame_timeout_ms=10000,.request_timeout_ms=30000,.simulation=true};
    if(!ag_device_endpoint_init(&f->endpoint,&f->admission,&f->store,&options) || !connect_peer(f,1))return 3;
    for(;;){
        int timeout=-1;
        if(f->endpoint.stream.used){
            uint64_t elapsed=now_ms()-f->endpoint.stream.started_ms;
            if(elapsed>=10000)return 4;
            timeout=(int)(10000-elapsed);
        }
        struct pollfd input={STDIN_FILENO,POLLIN,0};int ready=poll(&input,1,timeout);
        if(ready<0 && errno==EINTR)continue;
        if(ready<=0)return 4;
        if(f->endpoint.stream.used==0 && !control(f))return 4;
        uint8_t chunk[512];ssize_t count=read(STDIN_FILENO,chunk,sizeof(chunk));
        if(count<0 && errno==EINTR)continue;
        if(count<=0){if(f->endpoint.stream.used)return 4;break;}
        if(ag_device_endpoint_feed(&f->endpoint,&f->lease,chunk,(size_t)count,now_ms())!=AG_DEVICE_OK)return 4;
        while(f->endpoint.phase!=AG_DEVICE_IDLE){
            if(f->endpoint.output_length==0){ag_device_result_t step=ag_device_endpoint_step(&f->endpoint,now_ms());if(step!=AG_DEVICE_OK)return 4;}
            uint8_t output[AG_FRAME_MAX_SIZE];size_t written=0;
            if(ag_device_endpoint_take_output(&f->endpoint,&f->lease,now_ms(),output,sizeof(output),&written)!=AG_DEVICE_OK)return 4;
            ag_frame_view_t frame;if(ag_frame_decode(output,written,&frame)!=AG_FRAME_OK)return 4;
            bool applied=false;for(size_t i=0;i+9<=frame.payload_length;++i)if(memcmp(frame.payload+i,"\"applied\"",9)==0)applied=true;
            if(f->drop_applied && applied){f->drop_applied=false;continue;}
            if(!full_write(STDOUT_FILENO,output,written))return 4;
        }
    }
    if(f->fd>=0)close(f->fd);return 0;
}
