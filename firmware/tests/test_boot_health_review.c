/* Independent lifecycle model: production core, atomic-blob and SDK state doubles.
 * No NVS/SDK code or physical flash is executed by this program. */
#include "ag_boot_health.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define OLD_ADDRESS 0x10000u
#define NEW_ADDRESS 0x110000u
#define IMAGE_BYTES 2080u
static ag_boot_health_t owner;
static struct {
    uint8_t bytes[AG_BOOT_RECORD_BYTES];bool exists,write_error,write_commits,read_error,target_valid,mark_error,mark_effect,reentry;
    unsigned writes,marks,reentries,fail_write_number;
    ag_boot_observation_t image;
} f;
static uint64_t u64(const uint8_t *p){uint64_t n=0;for(unsigned i=0;i<8;++i)n|=(uint64_t)p[i]<<(8*i);return n;}
static void nested(void){
    if(!f.reentry)return;f.reentry=false;++f.reentries;
    assert(ag_boot_health_reconcile(&owner)==AG_BOOT_BUSY);
    assert(ag_boot_health_confirm(&owner,AG_BOOT_HEALTH_REQUIRED_MIN)==AG_BOOT_BUSY);
    uint8_t hash[32]={0x42};assert(ag_boot_health_stage(&owner,10,NEW_ADDRESS,IMAGE_BYTES,hash)==AG_BOOT_BUSY);
    assert(ag_boot_health_provision(&owner,0)==AG_BOOT_BUSY);
    uint64_t floor=123;assert(!ag_boot_health_floor(&owner,&floor)&&floor==123);
    f.reentry=true;
}
static ag_boot_load_t load(void *context,uint8_t out[96]){assert(context==&f);nested();if(f.read_error)return AG_BOOT_LOAD_IO;if(!f.exists)return AG_BOOT_LOAD_EMPTY;memcpy(out,f.bytes,96);return AG_BOOT_LOAD_OK;}
static bool replace(void *context,const uint8_t bytes[96]){
    assert(context==&f);nested();++f.writes;
    /* An accepted floor10 write must follow an actual SDKVALID observation. */
    if(u64(bytes+16)==10)assert(f.marks==1&&f.image.state==AG_BOOT_IMAGE_VALID);
    bool error=f.write_error&&f.writes==f.fail_write_number;
    if(!error||f.write_commits){memcpy(f.bytes,bytes,96);f.exists=true;}
    return !error;
}
static bool observe(void *context,uint32_t image_bytes,ag_boot_observation_t *out){assert(context==&f);nested();assert(image_bytes==0||image_bytes==IMAGE_BYTES);*out=f.image;return true;}
static bool target(void *context,uint32_t address,uint32_t image_bytes,const uint8_t digest[32]){assert(context==&f);nested();return f.target_valid&&address==NEW_ADDRESS&&image_bytes==IMAGE_BYTES&&digest[0]==0x42;}
static bool mark(void *context){assert(context==&f);nested();++f.marks;if(!f.mark_error||f.mark_effect)f.image.state=AG_BOOT_IMAGE_VALID;return !f.mark_error;}
static ag_boot_hooks_t hooks(void){return (ag_boot_hooks_t){&f,load,replace,observe,target,mark,false};}
static ag_boot_result_t reopen(uint32_t required){memset(&owner,0,sizeof(owner));ag_boot_hooks_t h=hooks();return ag_boot_health_open(&owner,&h,required);}
static void baseline(void){
    memset(&f,0,sizeof(f));f.target_valid=true;f.image=(ag_boot_observation_t){OLD_ADDRESS,OLD_ADDRESS,AG_BOOT_IMAGE_VALID,{0x42}};
    assert(reopen(15)==AG_BOOT_EMPTY);assert(ag_boot_health_provision(&owner,9)==AG_BOOT_OK&&f.writes==1);
}
static void stage(void){uint8_t hash[32]={0x42};assert(ag_boot_health_stage(&owner,10,NEW_ADDRESS,IMAGE_BYTES,hash)==AG_BOOT_OK&&f.writes==2);}
static void trial(void){baseline();stage();f.image.running_address=f.image.boot_address=NEW_ADDRESS;f.image.state=AG_BOOT_IMAGE_PENDING;}
static void floor_is(uint64_t expected){uint64_t value=UINT64_MAX;assert(ag_boot_health_floor(&owner,&value)&&value==expected);}
static void test_false_after_commit_boundaries(void){
    for(unsigned write=3;write<=4;++write)for(unsigned committed=0;committed<=1;++committed){
        trial();f.write_error=true;f.write_commits=committed;f.fail_write_number=write;
        assert(ag_boot_health_confirm(&owner,15)==AG_BOOT_UNKNOWN);assert(!ag_boot_health_floor(&owner,&(uint64_t){0}));
        if(write==3)assert(f.marks==0);else assert(f.marks==1);
        unsigned marks=f.marks;assert(ag_boot_health_confirm(&owner,15)==AG_BOOT_INVALID&&f.marks==marks);
        f.write_error=false;assert(reopen(15)==AG_BOOT_OK);
        if(write==4){assert(ag_boot_health_reconcile(&owner)==AG_BOOT_OK);floor_is(10);assert(f.marks==1);}
        else {f.image=(ag_boot_observation_t){OLD_ADDRESS,OLD_ADDRESS,AG_BOOT_IMAGE_VALID,{0x42}};
            assert(ag_boot_health_reconcile(&owner)==AG_BOOT_RECOVERED_FALLBACK);floor_is(9);assert(f.marks==0);}
    }
}
static void test_health_policy_survives_reopen(void){
    trial();f.mark_error=true;f.mark_effect=true;
    assert(ag_boot_health_confirm(&owner,15)==AG_BOOT_UNKNOWN&&f.marks==1&&f.image.state==AG_BOOT_IMAGE_VALID);
    f.mark_error=false;assert(reopen(255)==AG_BOOT_OK);
    ag_boot_result_t result=ag_boot_health_reconcile(&owner);
    assert(result!=AG_BOOT_OK);uint64_t floor;
    assert(!ag_boot_health_floor(&owner,&floor)||floor==9);assert(f.marks==1);
}
static void test_unexpected_valid_identity_and_reentry(void){
    trial();f.image.state=AG_BOOT_IMAGE_VALID;assert(ag_boot_health_reconcile(&owner)==AG_BOOT_POLICY&&f.marks==0);
    trial();f.image.digest[0]^=1;assert(ag_boot_health_confirm(&owner,15)==AG_BOOT_POLICY&&f.marks==0);
    baseline();f.reentry=true;stage();f.image.running_address=f.image.boot_address=NEW_ADDRESS;f.image.state=AG_BOOT_IMAGE_PENDING;
    assert(ag_boot_health_confirm(&owner,15)==AG_BOOT_OK);f.reentry=false;floor_is(10);assert(f.reentries>5&&f.marks==1);
}
int main(void){test_false_after_commit_boundaries();test_health_policy_survives_reopen();test_unexpected_valid_identity_and_reentry();puts("PASS: 3 independent boot-health groups; atomic-blob/SDK behavioral model only");return 0;}
