#include "ag_admission.h"
#include <string.h>

static void wipe_passkey(ag_admission_t *a) {
    volatile uint32_t *secret=&a->passkey; *secret=0;
    a->passkey_issued=false;
}
static bool current(const ag_admission_t *a,const ag_connection_lease_t *lease) {
    const ag_connection_t *c=a->connection;
    return lease!=NULL && a->link_open && c->initialized && c->boot_journal_loaded
        && !c->maintenance_blocked && c->state!=AG_CONNECTION_IDLE
        && lease->counter!=0 && lease->counter==a->lease.counter && lease->counter==c->lease_counter
        && memcmp(lease->boot_id,a->lease.boot_id,AG_SESSION_ID_BYTES)==0
        && memcmp(lease->boot_id,c->boot_id,AG_SESSION_ID_BYTES)==0;
}
static void drop_link(ag_admission_t *a) {
    if (current(a,&a->lease)) (void)ag_connection_close(a->connection,&a->lease);
    a->link_open=a->recognized=a->verification_in_progress=false;
    memset(&a->lease,0,sizeof(a->lease));wipe_passkey(a);
}
static bool bridge(void *context,const ag_client_id_t *client,const ag_connection_lease_t *lease,
                   const uint8_t *proof,size_t length) {
    ag_admission_t *a=context;
    return a!=NULL && a->initialized && a->verification_in_progress && current(a,lease)
        && proof!=NULL && length==1 && proof[0]==1
        && memcmp(client->bytes,a->connection->client.bytes,AG_CLIENT_ID_BYTES)==0;
}
static bool secure(const ag_link_security_t *s) {
    return s->encrypted && s->mitm_authenticated && s->secure_connections
        && s->identity_resolved && s->bonded && s->key_bits==128;
}
bool ag_admission_init(ag_admission_t *a,ag_connection_t *c,const ag_admission_config_t *config,
                        const ag_admission_hooks_t *hooks) {
    if(a==NULL) return false;
    if(c==NULL || config==NULL || hooks==NULL || !c->initialized || c->verifier!=NULL
        || c->state!=AG_CONNECTION_IDLE || config->window_ms==0 || config->window_ms>AG_ADMISSION_MAX_WINDOW_MS
        || config->auth_timeout_ms==0 || config->auth_timeout_ms>AG_ADMISSION_MAX_AUTH_MS
        || config->cooldown_ms==0 || config->cooldown_ms>AG_ADMISSION_MAX_COOLDOWN_MS
        || config->attempts==0 || config->attempts>AG_ADMISSION_MAX_ATTEMPTS
        || hooks->random_u32==NULL || hooks->lookup==NULL || hooks->read_security==NULL || hooks->confirm_bond==NULL)
        return false;
    const ag_admission_config_t cfg=*config; const ag_admission_hooks_t h=*hooks;
    memset(a,0,sizeof(*a));a->connection=c;a->config=cfg;a->hooks=h;a->initialized=true;
    c->verifier=bridge;c->verifier_context=a;
    return true;
}
ag_admission_result_t ag_admission_tick(ag_admission_t *a,uint64_t now) {
    if(a==NULL || !a->initialized) return AG_ADMISSION_INVALID;
    if(a->clock_fault) return AG_ADMISSION_CLOCK_FAULT;
    if(a->has_time && now<a->last_ms) {
        a->clock_fault=true;a->window_open=false;drop_link(a);return AG_ADMISSION_CLOCK_FAULT;
    }
    a->has_time=true;a->last_ms=now;
    if(a->connection->maintenance_blocked || !a->connection->boot_journal_loaded
        || a->connection->verifier!=bridge || a->connection->verifier_context!=a) {
        a->window_open=false;drop_link(a);return AG_ADMISSION_REJECTED;
    }
    if(a->link_open && !current(a,&a->lease)) drop_link(a);
    if(a->cooling_down && now-a->cooldown_started_ms>=a->config.cooldown_ms) a->cooling_down=false;
    if(a->window_open && now-a->window_started_ms>=a->config.window_ms) {
        a->window_open=false;
        if(a->link_open && !a->recognized) {drop_link(a);return AG_ADMISSION_TIMEOUT;}
    }
    if(a->link_open && a->connection->state!=AG_CONNECTION_VERIFIED
        && now-a->connected_ms>=a->config.auth_timeout_ms) {
        drop_link(a);return AG_ADMISSION_TIMEOUT;
    }
    return AG_ADMISSION_OK;
}
ag_admission_result_t ag_admission_open_window(ag_admission_t *a,bool local,uint64_t now) {
    ag_admission_result_t result=ag_admission_tick(a,now);if(result!=AG_ADMISSION_OK) return result;
    if(!local) return AG_ADMISSION_NOT_LOCAL;
    if(a->connection->state!=AG_CONNECTION_IDLE) return AG_ADMISSION_BUSY;
    if(a->cooling_down) return AG_ADMISSION_COOLDOWN;
    a->window_open=true;a->window_started_ms=now;a->attempts_used=0;wipe_passkey(a);
    return AG_ADMISSION_OK;
}
ag_admission_result_t ag_admission_connect(ag_admission_t *a,const ag_client_id_t *client,uint64_t now,
                                           ag_connection_lease_t *out) {
    if(out!=NULL) memset(out,0,sizeof(*out));
    ag_admission_result_t result=ag_admission_tick(a,now);if(result!=AG_ADMISSION_OK) return result;
    if(client==NULL || out==NULL) return AG_ADMISSION_INVALID;
    if(a->connection->state!=AG_CONNECTION_IDLE) return AG_ADMISSION_BUSY;
    ag_bond_lookup_t known=a->hooks.lookup(a->hooks.context,client);
    if(known!=AG_BOND_RECOGNIZED && known!=AG_BOND_UNKNOWN) return AG_ADMISSION_REJECTED;
    if(known==AG_BOND_UNKNOWN) {
        if(a->cooling_down) return AG_ADMISSION_COOLDOWN;
        if(!a->window_open) return AG_ADMISSION_WINDOW_CLOSED;
        if(a->attempts_used>=a->config.attempts) {
            a->window_open=false;a->cooling_down=true;a->cooldown_started_ms=now;return AG_ADMISSION_COOLDOWN;
        }
    }
    if(ag_connection_open(a->connection,client,out)!=AG_CONNECTION_OK) return AG_ADMISSION_REJECTED;
    a->lease=*out;a->link_open=true;a->recognized=known==AG_BOND_RECOGNIZED;a->connected_ms=now;
    if(!a->recognized) ++a->attempts_used;
    wipe_passkey(a);return AG_ADMISSION_OK;
}
ag_admission_result_t ag_admission_passkey(ag_admission_t *a,const ag_connection_lease_t *lease,
                                           uint64_t now,char display[7]) {
    if(display!=NULL) memset(display,0,7);
    ag_admission_result_t result=ag_admission_tick(a,now);if(result!=AG_ADMISSION_OK) return result;
    if(display==NULL) return AG_ADMISSION_INVALID;
    if(!current(a,lease)) return AG_ADMISSION_STALE;
    if(a->recognized || !a->window_open || a->connection->state!=AG_CONNECTION_UNVERIFIED) return AG_ADMISSION_REJECTED;
    if(!a->passkey_issued) {
        bool accepted=false;
        for(unsigned i=0;i<AG_ADMISSION_ENTROPY_TRIES;++i) {
            uint32_t random=0;
            if(!a->hooks.random_u32(a->hooks.context,&random)) break;
            /* Largest multiple of 1e6 below 2^32: uniform, no modulo bias. */
            if(random<UINT32_C(4294000000)) {a->passkey=random%UINT32_C(1000000);accepted=true;break;}
        }
        if(!accepted) {a->window_open=false;drop_link(a);return AG_ADMISSION_ENTROPY_FAILED;}
        a->passkey_issued=true;
    }
    uint32_t digits=a->passkey;
    for(unsigned i=6;i>0;--i){display[i-1]=(char)('0'+digits%10);digits/=10;}
    display[6]='\0';return AG_ADMISSION_OK;
}
ag_admission_result_t ag_admission_security_changed(ag_admission_t *a,const ag_connection_lease_t *lease,uint64_t now) {
    ag_admission_result_t result=ag_admission_tick(a,now);if(result!=AG_ADMISSION_OK) return result;
    if(!current(a,lease)) return AG_ADMISSION_STALE;
    if(!a->recognized && (!a->window_open || !a->passkey_issued)) {drop_link(a);return AG_ADMISSION_REJECTED;}
    ag_link_security_t evidence={0};
    if(!a->hooks.read_security(a->hooks.context,&a->connection->client,lease,&evidence) || !secure(&evidence)
        || (!a->recognized && !evidence.current_display_passkey_used)) {
        drop_link(a);return AG_ADMISSION_REJECTED;
    }
    if(!a->recognized && !a->hooks.confirm_bond(a->hooks.context,&a->connection->client,lease)) {
        drop_link(a);return AG_ADMISSION_STORAGE_FAILED;
    }
    a->verification_in_progress=true;const uint8_t proof=1;
    ag_connection_result_t verified=ag_connection_authenticate(a->connection,lease,&proof,1);
    a->verification_in_progress=false;
    if(verified!=AG_CONNECTION_OK) {drop_link(a);return AG_ADMISSION_REJECTED;}
    a->recognized=true;a->window_open=false;wipe_passkey(a);return AG_ADMISSION_OK;
}
bool ag_admission_private_allowed(ag_admission_t *a,const ag_connection_lease_t *lease,uint64_t now) {
    if(ag_admission_tick(a,now)!=AG_ADMISSION_OK || !current(a,lease)
        || !ag_connection_mutations_allowed(a->connection,lease)) return false;
    ag_link_security_t evidence={0};
    if(!a->hooks.read_security(a->hooks.context,&a->connection->client,lease,&evidence) || !secure(&evidence)) {
        drop_link(a);return false;
    }
    return true;
}
ag_admission_result_t ag_admission_disconnect(ag_admission_t *a,const ag_connection_lease_t *lease,uint64_t now) {
    ag_admission_result_t result=ag_admission_tick(a,now);if(result!=AG_ADMISSION_OK) return result;
    if(!current(a,lease)) return AG_ADMISSION_STALE;
    drop_link(a);return AG_ADMISSION_OK;
}
ag_admission_result_t ag_admission_repeat_pairing(ag_admission_t *a,const ag_connection_lease_t *lease,uint64_t now) {
    ag_admission_result_t result=ag_admission_tick(a,now);if(result!=AG_ADMISSION_OK) return result;
    if(!current(a,lease)) return AG_ADMISSION_STALE;
    /* No remove-bond callback. Future adapter replies pairing-ignore, not retry. */
    return AG_ADMISSION_REJECTED;
}
