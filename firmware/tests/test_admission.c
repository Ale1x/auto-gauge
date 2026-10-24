#include "ag_admission.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    bool known[4], entropy_ok, security_ok, storage_ok, lookup_ok;
    uint32_t randoms[AG_ADMISSION_ENTROPY_TRIES+1];size_t random_count, random_index;
    unsigned confirmations, reads;
    ag_link_security_t evidence;
    ag_client_id_t bound_client;ag_connection_lease_t bound_lease;
} fixture_t;
typedef struct {ag_admission_t a;ag_connection_t c;ag_factory_reset_t reset;fixture_t f;} test_t;
static ag_client_id_t client(uint8_t id) {ag_client_id_t c={{0}};c.bytes[0]=id;return c;}
static bool random_u32(void *v,uint32_t *out) {
    fixture_t *f=v;if(!f->entropy_ok)return false;
    if(f->random_index>=f->random_count)return false;
    *out=f->randoms[f->random_index++];return true;
}
static ag_bond_lookup_t lookup(void *v,const ag_client_id_t *c) {
    fixture_t *f=v;if(!f->lookup_ok || c->bytes[0]>=4)return AG_BOND_LOOKUP_FAILED;
    return f->known[c->bytes[0]]?AG_BOND_RECOGNIZED:AG_BOND_UNKNOWN;
}
static bool read_security(void *v,const ag_client_id_t *c,const ag_connection_lease_t *l,ag_link_security_t *s) {
    fixture_t *f=v;++f->reads;
    if(!f->security_ok || memcmp(c,&f->bound_client,sizeof(*c))!=0
        || l->counter!=f->bound_lease.counter || memcmp(l->boot_id,f->bound_lease.boot_id,16)!=0)return false;
    *s=f->evidence;return true;
}
static bool confirm(void *v,const ag_client_id_t *c,const ag_connection_lease_t *l) {
    fixture_t *f=v;(void)l;++f->confirmations;
    if(!f->storage_ok)return false;
    f->known[c->bytes[0]]=true;return true;
}
static void init(test_t *t) {
    memset(t,0,sizeof(*t));uint8_t boot[16]={1};
    assert(ag_connection_init(&t->c,boot,NULL,NULL));
    ag_reset_snapshot_t snapshot=ag_reset_clean_snapshot();
    assert(ag_factory_reset_restore(&t->reset,&t->c,NULL,true,&snapshot));
    t->f.entropy_ok=t->f.security_ok=t->f.storage_ok=t->f.lookup_ok=true;
    t->f.evidence=(ag_link_security_t){true,true,true,true,true,128,true};
    t->f.randoms[0]=123456;t->f.randoms[1]=654321;t->f.random_count=2;
    ag_admission_config_t config={1000,100,500,2};
    ag_admission_hooks_t hooks={&t->f,random_u32,lookup,read_security,confirm};
    assert(ag_admission_init(&t->a,&t->c,&config,&hooks));
}
static ag_connection_lease_t connect(test_t *t,uint8_t id,uint64_t now) {
    ag_client_id_t c=client(id);ag_connection_lease_t l;
    assert(ag_admission_connect(&t->a,&c,now,&l)==AG_ADMISSION_OK);
    t->f.bound_client=c;t->f.bound_lease=l;
    assert(!ag_connection_mutations_allowed(&t->c,&l));
    assert(!ag_admission_private_allowed(&t->a,&l,now));
    return l;
}
static void admit_new(test_t *t,uint8_t id,uint64_t now,ag_connection_lease_t *l) {
    assert(ag_admission_open_window(&t->a,true,now)==AG_ADMISSION_OK);
    *l=connect(t,id,now);char display[7];
    assert(ag_admission_passkey(&t->a,l,now,display)==AG_ADMISSION_OK);
    assert(ag_admission_security_changed(&t->a,l,now)==AG_ADMISSION_OK);
    assert(ag_admission_private_allowed(&t->a,l,now));
    assert(!t->a.passkey_issued && t->a.passkey==0 && !t->a.window_open);
}
static void test_sequence_busy_and_stale_no_erase(void) {
    test_t t;init(&t);ag_connection_lease_t a,b,again;admit_new(&t,1,0,&a);
    ag_client_id_t peer=client(2);memset(&b,0xff,sizeof(b));
    assert(ag_admission_connect(&t.a,&peer,1,&b)==AG_ADMISSION_BUSY && b.counter==0);
    assert(ag_admission_open_window(&t.a,true,1)==AG_ADMISSION_BUSY);
    assert(ag_admission_private_allowed(&t.a,&a,1));
    assert(ag_admission_disconnect(&t.a,&a,2)==AG_ADMISSION_OK);
    admit_new(&t,2,3,&b);
    assert(ag_admission_security_changed(&t.a,&a,4)==AG_ADMISSION_STALE);
    assert(ag_admission_disconnect(&t.a,&a,4)==AG_ADMISSION_STALE);
    assert(ag_admission_private_allowed(&t.a,&b,4));
    assert(ag_admission_disconnect(&t.a,&b,5)==AG_ADMISSION_OK);
    again=connect(&t,1,6);char display[7];
    assert(ag_admission_passkey(&t.a,&again,6,display)==AG_ADMISSION_REJECTED && display[0]==0);
    assert(ag_admission_security_changed(&t.a,&again,6)==AG_ADMISSION_OK);
    assert(ag_admission_private_allowed(&t.a,&again,6) && t.f.confirmations==2 && t.f.random_index==2);
    assert(again.counter!=a.counter && t.f.known[1] && t.f.known[2]);
}
static void test_window_entropy_and_leading_zero(void) {
    test_t t;init(&t);ag_connection_lease_t l;ag_client_id_t c=client(1);
    assert(ag_admission_connect(&t.a,&c,0,&l)==AG_ADMISSION_WINDOW_CLOSED);
    assert(ag_admission_open_window(&t.a,false,0)==AG_ADMISSION_NOT_LOCAL);
    assert(ag_admission_open_window(&t.a,true,0)==AG_ADMISSION_OK);l=connect(&t,1,0);
    t.f.randoms[0]=UINT32_MAX;t.f.randoms[1]=42;char display[7];
    assert(ag_admission_passkey(&t.a,&l,0,display)==AG_ADMISSION_OK);
    assert(strcmp(display,"000042")==0 && t.f.random_index==2);
    assert(ag_admission_passkey(&t.a,&l,0,display)==AG_ADMISSION_OK && t.f.random_index==2);
    assert(ag_admission_disconnect(&t.a,&l,1)==AG_ADMISSION_OK && !t.a.passkey_issued);
    for(unsigned mode=0;mode<2;++mode){
        init(&t);assert(ag_admission_open_window(&t.a,true,0)==AG_ADMISSION_OK);l=connect(&t,1,0);
        t.f.entropy_ok=mode!=0;t.f.random_count=AG_ADMISSION_ENTROPY_TRIES;
        for(unsigned i=0;i<AG_ADMISSION_ENTROPY_TRIES;++i)t.f.randoms[i]=UINT32_C(4294000000);
        assert(ag_admission_passkey(&t.a,&l,0,display)==AG_ADMISSION_ENTROPY_FAILED);
        assert(display[0]==0 && !t.a.link_open && !t.a.window_open && t.a.passkey==0);
        assert(!ag_admission_private_allowed(&t.a,&l,1));
    }
}
static void test_downgrade_missing_security_and_no_fake_auth(void) {
    for(unsigned field=0;field<7;++field){
        test_t t;init(&t);t.f.known[1]=true;ag_connection_lease_t l=connect(&t,1,0);
        const uint8_t fake=1;
        assert(ag_connection_authenticate(&t.c,&l,&fake,1)==AG_CONNECTION_AUTH_REJECTED);
        switch(field){case 0:t.f.evidence.encrypted=false;break;case 1:t.f.evidence.mitm_authenticated=false;break;
        case 2:t.f.evidence.secure_connections=false;break;case 3:t.f.evidence.identity_resolved=false;break;
        case 4:t.f.evidence.bonded=false;break;case 5:t.f.evidence.key_bits=127;break;default:t.f.security_ok=false;break;}
        assert(ag_admission_security_changed(&t.a,&l,1)==AG_ADMISSION_REJECTED);
        assert(!ag_connection_mutations_allowed(&t.c,&l) && t.c.state==AG_CONNECTION_IDLE);
    }
    test_t t;init(&t);ag_connection_lease_t l;admit_new(&t,1,0,&l);
    t.f.evidence.encrypted=false;assert(!ag_admission_private_allowed(&t.a,&l,1));
    assert(t.c.state==AG_CONNECTION_IDLE && !t.a.link_open);
    init(&t);assert(ag_admission_open_window(&t.a,true,0)==AG_ADMISSION_OK);l=connect(&t,1,0);
    assert(ag_admission_security_changed(&t.a,&l,1)==AG_ADMISSION_REJECTED); /* No passkey event. */
    init(&t);assert(ag_admission_open_window(&t.a,true,0)==AG_ADMISSION_OK);l=connect(&t,1,0);
    char display[7];assert(ag_admission_passkey(&t.a,&l,0,display)==AG_ADMISSION_OK);
    t.f.evidence.current_display_passkey_used=false;
    assert(ag_admission_security_changed(&t.a,&l,1)==AG_ADMISSION_REJECTED); /* MITM via another method is insufficient. */
}
static void test_timeouts_limits_cooldown_and_clock(void) {
    test_t t;init(&t);t.f.known[1]=true;ag_connection_lease_t l=connect(&t,1,0);
    assert(ag_admission_tick(&t.a,100)==AG_ADMISSION_TIMEOUT && !t.a.link_open);
    init(&t);assert(ag_admission_open_window(&t.a,true,0)==AG_ADMISSION_OK);
    assert(ag_admission_tick(&t.a,1000)==AG_ADMISSION_OK && !t.a.window_open);
    init(&t);assert(ag_admission_open_window(&t.a,true,0)==AG_ADMISSION_OK);
    for(unsigned i=0;i<2;++i){l=connect(&t,1,i);assert(ag_admission_disconnect(&t.a,&l,i)==AG_ADMISSION_OK);}
    ag_client_id_t c=client(1);
    assert(ag_admission_connect(&t.a,&c,2,&l)==AG_ADMISSION_COOLDOWN);
    assert(ag_admission_open_window(&t.a,true,501)==AG_ADMISSION_COOLDOWN);
    t.f.known[2]=true;
    ag_connection_lease_t recognized=connect(&t,2,501);
    assert(ag_admission_security_changed(&t.a,&recognized,501)==AG_ADMISSION_OK);
    assert(ag_admission_disconnect(&t.a,&recognized,501)==AG_ADMISSION_OK);
    assert(ag_admission_open_window(&t.a,true,502)==AG_ADMISSION_OK);
    l=connect(&t,1,502);
    assert(ag_admission_tick(&t.a,501)==AG_ADMISSION_CLOCK_FAULT);
    assert(!t.a.link_open && !t.a.window_open && ag_admission_tick(&t.a,503)==AG_ADMISSION_CLOCK_FAULT);
    init(&t);assert(ag_admission_open_window(&t.a,true,UINT64_MAX-5)==AG_ADMISSION_OK);
    l=connect(&t,1,UINT64_MAX-4);
    assert(ag_admission_tick(&t.a,UINT64_MAX)==AG_ADMISSION_OK);
    assert(ag_admission_tick(&t.a,0)==AG_ADMISSION_CLOCK_FAULT);
    init(&t);t.c.lease_counter=UINT64_MAX;
    assert(ag_admission_open_window(&t.a,true,0)==AG_ADMISSION_OK);
    assert(ag_admission_connect(&t.a,&c,0,&l)==AG_ADMISSION_REJECTED && l.counter==0);
    init(&t);t.a.config.window_ms=5;
    assert(ag_admission_open_window(&t.a,true,0)==AG_ADMISSION_OK);l=connect(&t,1,0);
    char display[7];assert(ag_admission_passkey(&t.a,&l,0,display)==AG_ADMISSION_OK);
    assert(ag_admission_tick(&t.a,5)==AG_ADMISSION_TIMEOUT && t.a.passkey==0 && !t.a.link_open);
}
static void test_bond_loss_storage_full_repeat_and_reset(void) {
    test_t t;init(&t);ag_connection_lease_t l;admit_new(&t,1,0,&l);
    assert(ag_admission_repeat_pairing(&t.a,&l,1)==AG_ADMISSION_REJECTED);
    assert(t.f.known[1] && ag_admission_private_allowed(&t.a,&l,1));
    assert(ag_admission_disconnect(&t.a,&l,2)==AG_ADMISSION_OK);
    t.f.known[1]=false;ag_client_id_t c=client(1);
    assert(ag_admission_connect(&t.a,&c,3,&l)==AG_ADMISSION_WINDOW_CLOSED);
    admit_new(&t,1,3,&l);assert(t.f.confirmations==2);
    t.c.maintenance_blocked=true;t.c.state=AG_CONNECTION_IDLE;
    assert(ag_admission_tick(&t.a,4)==AG_ADMISSION_REJECTED);
    assert(!t.a.link_open && !t.a.window_open && t.a.passkey==0);
    assert(!ag_admission_private_allowed(&t.a,&l,4));
    init(&t);assert(ag_admission_open_window(&t.a,true,0)==AG_ADMISSION_OK);l=connect(&t,1,0);
    char pending[7];assert(ag_admission_passkey(&t.a,&l,0,pending)==AG_ADMISSION_OK);
    t.c.maintenance_blocked=true;t.c.state=AG_CONNECTION_IDLE;
    assert(ag_admission_tick(&t.a,1)==AG_ADMISSION_REJECTED && t.a.passkey==0 && !t.a.passkey_issued);
    init(&t);assert(ag_admission_open_window(&t.a,true,0)==AG_ADMISSION_OK);l=connect(&t,1,0);
    char display[7];assert(ag_admission_passkey(&t.a,&l,0,display)==AG_ADMISSION_OK);
    t.f.storage_ok=false;
    assert(ag_admission_security_changed(&t.a,&l,1)==AG_ADMISSION_STORAGE_FAILED && !t.a.link_open);
    assert(!t.f.known[1] && t.f.confirmations==1);
    init(&t);t.f.lookup_ok=false;
    assert(ag_admission_connect(&t.a,&c,0,&l)==AG_ADMISSION_REJECTED);
}
static void test_init_requires_hooks_bounds_and_journal(void) {
    test_t t;init(&t);ag_admission_t other;
    assert(!ag_admission_init(&other,&t.c,&t.a.config,&t.a.hooks)); /* Cannot replace bridge. */
    uint8_t boot[16]={1};assert(ag_connection_init(&t.c,boot,NULL,NULL));
    ag_admission_hooks_t h=t.a.hooks;h.random_u32=NULL;
    assert(!ag_admission_init(&other,&t.c,&t.a.config,&h));
    h=t.a.hooks;ag_admission_config_t bad=t.a.config;bad.attempts=0;
    assert(!ag_admission_init(&other,&t.c,&bad,&h));
    bad=t.a.config;bad.window_ms=AG_ADMISSION_MAX_WINDOW_MS+1;
    assert(!ag_admission_init(&other,&t.c,&bad,&h));
    assert(ag_admission_init(&other,&t.c,&t.a.config,&h));
    assert(ag_admission_open_window(&other,true,0)==AG_ADMISSION_REJECTED); /* Journal not restored. */
}
int main(void) {
    test_sequence_busy_and_stale_no_erase();test_window_entropy_and_leading_zero();
    test_downgrade_missing_security_and_no_fake_auth();test_timeouts_limits_cooldown_and_clock();
    test_bond_loss_storage_full_repeat_and_reset();test_init_requires_hooks_bounds_and_journal();
    puts("PASS: 6 admission groups; local window, trusted security, unbiased entropy, N sequential clients, no radio/erase");
    return 0;
}
