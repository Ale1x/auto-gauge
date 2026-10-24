#include "manifest_verify.h"
#include "psa/crypto.h"
#include <string.h>

typedef struct {const uint8_t *bytes;size_t length,at;} parser_t;
typedef struct {
    char hardware[65],version[32],counter[21],digest[65],key_id[65];
    uint64_t release;uint32_t size;
    uint8_t signature[384];
} parsed_t;
static void ws(parser_t *p){while(p->at<p->length&&(p->bytes[p->at]==' '||p->bytes[p->at]=='\n'||p->bytes[p->at]=='\r'||p->bytes[p->at]=='\t'))++p->at;}
static bool token(parser_t *p,uint8_t c){ws(p);if(p->at>=p->length||p->bytes[p->at]!=c)return false;++p->at;return true;}
static int hex(uint8_t c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
/* Every schema string is printable ASCII. Decode before key matching so aliases
 * such as schema_versi\u006fn cannot evade duplicate detection. */
static bool string(parser_t *p,char *out,size_t capacity){
    if(!token(p,'"'))return false;
    size_t used=0;
    while(p->at<p->length){uint8_t c=p->bytes[p->at++];
        if(c=='"'){if(used>=capacity)return false;out[used]=0;return true;}
        if(c=='\\'){
            if(p->at>=p->length)return false;
            c=p->bytes[p->at++];
            if(c=='u'){unsigned value=0;if(p->length-p->at<4)return false;
                for(unsigned i=0;i<4;++i){int n=hex(p->bytes[p->at++]);if(n<0)return false;value=value*16+(unsigned)n;}
                if(value>126||value<32)return false;
                c=(uint8_t)value;
            }else if(c!='"'&&c!='\\'&&c!='/')return false;
        }
        if(c<32||c>126||used+1>=capacity)return false;
        out[used++]=(char)c;
    }
    return false;
}
static bool number(parser_t *p,uint32_t *out){
    ws(p);if(p->at>=p->length||p->bytes[p->at]<'0'||p->bytes[p->at]>'9')return false;
    uint32_t value=0;bool zero=p->bytes[p->at]=='0';size_t start=p->at;
    while(p->at<p->length&&p->bytes[p->at]>='0'&&p->bytes[p->at]<='9'){
        uint32_t digit=p->bytes[p->at++]-'0';if(value>(UINT32_MAX-digit)/10)return false;value=value*10+digit;
    }
    if(zero&&p->at-start!=1)return false;
    *out=value;return true;
}
static bool identifier(const char *s,bool version,size_t maximum){
    size_t n=0;for(;n<=maximum&&s[n];++n){unsigned char c=(unsigned char)s[n];
        bool alpha=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');
        if(!alpha&&(n==0||(c!='.'&&c!='_'&&c!='-'&&(!version||c!='+'))))return false;
    }
    return n>0&&n<=maximum;
}
static bool counter(const char *s,uint64_t *out){
    size_t n=strlen(s);if(n==0||n>20||(n>1&&s[0]=='0'))return false;
    uint64_t v=0;for(size_t i=0;i<n;++i){if(s[i]<'0'||s[i]>'9')return false;uint64_t d=(uint64_t)(s[i]-'0');if(v>(UINT64_MAX-d)/10)return false;v=v*10+d;}
    *out=v;return true;
}
static int b64(unsigned char c){if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;}
static bool signature(const char *s,uint8_t out[384]){
    if(strlen(s)!=512)return false;
    for(size_t i=0;i<128;++i){int a=b64((unsigned char)s[i*4]),b=b64((unsigned char)s[i*4+1]),c=b64((unsigned char)s[i*4+2]),d=b64((unsigned char)s[i*4+3]);
        if(a<0||b<0||c<0||d<0)return false;
        out[i*3]=(uint8_t)((a<<2)|(b>>4));out[i*3+1]=(uint8_t)((b<<4)|(c>>2));out[i*3+2]=(uint8_t)((c<<6)|d);
    }
    return true; /* 384 is divisible by3: exactly512 symbols, no padding bits. */
}
static bool manifest(parser_t *p,parsed_t *m){
    static const char *const keys[]={"schema_version","target","hardware_id","firmware_version","release_counter","image_bytes","image_sha256","signing_key_id"};
    if(!token(p,'{'))return false;
    unsigned seen=0;
    for(unsigned field=0;field<8;++field){char key[32],text[32];if(field&&!token(p,','))return false;
        if(!string(p,key,sizeof(key))||!token(p,':'))return false;
        unsigned which=0;while(which<8&&strcmp(key,keys[which]))++which;
        if(which==8||(seen&(1u<<which)))return false;
        seen|=1u<<which;
        uint32_t value;
        switch(which){
            case 0:if(!number(p,&value)||value!=1)return false;break;
            case 1:if(!string(p,text,sizeof(text))||strcmp(text,"esp32s3"))return false;break;
            case 2:if(!string(p,m->hardware,sizeof(m->hardware))||!identifier(m->hardware,false,64))return false;break;
            case 3:if(!string(p,m->version,sizeof(m->version))||!identifier(m->version,true,31))return false;break;
            case 4:if(!string(p,m->counter,sizeof(m->counter))||!counter(m->counter,&m->release))return false;break;
            case 5:if(!number(p,&m->size)||m->size==0||m->size>AG_OTA_IMAGE_MAX)return false;break;
            case 6:if(!string(p,m->digest,sizeof(m->digest))||strlen(m->digest)!=64)return false;
                for(unsigned i=0;i<64;++i){if(!((m->digest[i]>='0'&&m->digest[i]<='9')||(m->digest[i]>='a'&&m->digest[i]<='f')))return false;}
                break;
            case 7:if(!string(p,m->key_id,sizeof(m->key_id))||!identifier(m->key_id,false,64))return false;break;
            default:return false;
        }
    }
    return seen==255&&token(p,'}');
}
static bool envelope(const uint8_t *bytes,size_t length,parsed_t *out){
    parser_t p={bytes,length,0};unsigned seen=0;if(!token(&p,'{'))return false;
    for(unsigned i=0;i<3;++i){char key[32];if(i&&!token(&p,','))return false;
        if(!string(&p,key,sizeof(key))||!token(&p,':'))return false;
        unsigned bit;
        if(!strcmp(key,"manifest")){bit=1;if(seen&bit||!manifest(&p,out))return false;}
        else if(!strcmp(key,"signature_algorithm")){char algorithm[32];bit=2;if(seen&bit||!string(&p,algorithm,sizeof(algorithm))||strcmp(algorithm,"RSA-PSS-SHA256"))return false;}
        else if(!strcmp(key,"signature")){char encoded[513];bit=4;if(seen&bit||!string(&p,encoded,sizeof(encoded))||!signature(encoded,out->signature))return false;}
        else return false;
        seen|=bit;
    }
    if(seen!=7||!token(&p,'}'))return false;
    ws(&p);return p.at==p.length;
}
typedef struct {uint8_t bytes[AG_MANIFEST_SIGNING_MAX];size_t length;bool ok;} writer_t;
static void put(writer_t *w,const void *bytes,size_t n){if(!w->ok||n>sizeof(w->bytes)-w->length){w->ok=false;return;}memcpy(w->bytes+w->length,bytes,n);w->length+=n;}
static void text(writer_t *w,const char *s){put(w,s,strlen(s));}
static void decimal(writer_t *w,uint32_t n){char b[10];size_t count=0;do{b[count++]=(char)('0'+n%10);n/=10;}while(n);while(count)put(w,&b[--count],1);}
static void canonical(const parsed_t *m,writer_t *w){
    static const uint8_t domain[]="AUTO-GAUGE-OTA-MANIFEST\0V1\0";
    w->length=0;w->ok=true;put(w,domain,sizeof(domain)-1);
    text(w,"{\"firmware_version\":\"");text(w,m->version);text(w,"\",\"hardware_id\":\"");text(w,m->hardware);
    text(w,"\",\"image_bytes\":");decimal(w,m->size);text(w,",\"image_sha256\":\"");text(w,m->digest);
    text(w,"\",\"release_counter\":\"");text(w,m->counter);text(w,"\",\"schema_version\":1,\"signing_key_id\":\"");text(w,m->key_id);text(w,"\",\"target\":\"esp32s3\"}");
}
/* Strict bounded DER framing only, not RSA arithmetic. PSA remains responsible
 * for cryptographic key validation and signature verification. */
static bool der_item(const uint8_t *der,size_t length,size_t *at,uint8_t tag,size_t *value_at,size_t *value_length){
    if(*at>=length||der[(*at)++]!=tag||*at>=length)return false;
    size_t count=der[(*at)++];
    if(count&128u){size_t bytes=count&127u;if(bytes==0||bytes>2||bytes>length-*at||der[*at]==0)return false;
        count=0;for(size_t i=0;i<bytes;++i)count=count*256+der[(*at)++];
        if(count<128||(bytes==2&&count<256))return false;
    }
    if(count>length-*at)return false;
    *value_at=*at;*value_length=count;*at+=count;return true;
}
static bool positive_integer(const uint8_t *v,size_t n){
    if(n==0||(v[0]&128u))return false;
    if(n==1)return v[0]!=0;
    return v[0]!=0||(v[1]&128u)!=0;
}
static bool rsa_der(const uint8_t *der,size_t length){
    size_t at=0,start,n;if(!der_item(der,length,&at,0x30,&start,&n)||at!=length)return false;
    size_t end=at;at=start;size_t modulus,modulus_length;
    if(!der_item(der,end,&at,0x02,&modulus,&modulus_length)||modulus_length!=385
        ||der[modulus]!=0||(der[modulus+1]&128u)==0)return false;
    size_t exponent,exponent_length;
    if(!der_item(der,end,&at,0x02,&exponent,&exponent_length)||at!=end||!positive_integer(der+exponent,exponent_length))return false;
    return true;
}
ag_manifest_result_t ag_manifest_verify(const ag_manifest_trust_t *trust,const uint8_t *bytes,size_t length,ag_manifest_verified_t *output){
    if(trust==NULL||bytes==NULL||output==NULL||length==0||length>AG_MANIFEST_ENVELOPE_MAX)return AG_MANIFEST_INVALID;
    /* memchr first bounds local trust strings before identifier scans/strcmp. */
    if(memchr(trust->hardware_id,0,sizeof(trust->hardware_id))==NULL||!identifier(trust->hardware_id,false,64)
        ||trust->lookup_key==NULL||trust->inactive_capacity==0||trust->inactive_capacity>AG_OTA_IMAGE_MAX)return AG_MANIFEST_POLICY;
    parsed_t parsed;memset(&parsed,0,sizeof(parsed));
    if(!envelope(bytes,length,&parsed))return AG_MANIFEST_INVALID;
    if(strcmp(parsed.hardware,trust->hardware_id)||parsed.release<=trust->accepted_release_floor||parsed.size>trust->inactive_capacity||parsed.size%16!=0)return AG_MANIFEST_POLICY;
    uint8_t der[AG_MANIFEST_KEY_MAX];size_t der_length=trust->lookup_key(trust->key_context,parsed.key_id,der,sizeof(der));
    if(der_length==0||der_length>sizeof(der)||!rsa_der(der,der_length))return AG_MANIFEST_KEY;
    writer_t signed_bytes;canonical(&parsed,&signed_bytes);if(!signed_bytes.ok)return AG_MANIFEST_INVALID;
    uint8_t digest[32];size_t digest_length=0;
    if(psa_crypto_init()!=PSA_SUCCESS||psa_hash_compute(PSA_ALG_SHA_256,signed_bytes.bytes,signed_bytes.length,digest,sizeof(digest),&digest_length)!=PSA_SUCCESS||digest_length!=32)return AG_MANIFEST_CRYPTO;
    psa_key_attributes_t attributes=PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_lifetime(&attributes,PSA_KEY_LIFETIME_VOLATILE);
    psa_set_key_type(&attributes,PSA_KEY_TYPE_RSA_PUBLIC_KEY);psa_set_key_bits(&attributes,3072);
    psa_set_key_usage_flags(&attributes,PSA_KEY_USAGE_VERIFY_HASH);psa_set_key_algorithm(&attributes,PSA_ALG_RSA_PSS(PSA_ALG_SHA_256));
    psa_key_id_t key=0;psa_status_t imported=psa_import_key(&attributes,der,der_length,&key);psa_reset_key_attributes(&attributes);
    if(imported!=PSA_SUCCESS){if(key!=0&&psa_destroy_key(key)!=PSA_SUCCESS)return AG_MANIFEST_CLEANUP;return AG_MANIFEST_KEY;}
    if(key==0)return AG_MANIFEST_KEY;
    psa_key_attributes_t actual=PSA_KEY_ATTRIBUTES_INIT;ag_manifest_result_t result=AG_MANIFEST_OK;
    if(psa_get_key_attributes(key,&actual)!=PSA_SUCCESS||psa_get_key_type(&actual)!=PSA_KEY_TYPE_RSA_PUBLIC_KEY||psa_get_key_bits(&actual)!=3072)result=AG_MANIFEST_KEY;
    psa_reset_key_attributes(&actual);
    if(result==AG_MANIFEST_OK&&psa_verify_hash(key,PSA_ALG_RSA_PSS(PSA_ALG_SHA_256),digest,sizeof(digest),parsed.signature,sizeof(parsed.signature))!=PSA_SUCCESS)result=AG_MANIFEST_SIGNATURE;
    if(psa_destroy_key(key)!=PSA_SUCCESS)return AG_MANIFEST_CLEANUP;
    if(result!=AG_MANIFEST_OK)return result;
    ag_manifest_verified_t verified;memset(&verified,0,sizeof(verified));verified.plan.image_bytes=parsed.size;verified.plan.minimum_secure_version=trust->minimum_secure_version;
    memcpy(verified.plan.firmware_version,parsed.version,sizeof(parsed.version));
    for(unsigned i=0;i<32;++i)verified.plan.sha256[i]=(uint8_t)((hex((uint8_t)parsed.digest[i*2])<<4)|hex((uint8_t)parsed.digest[i*2+1]));
    verified.release_counter=parsed.release;memcpy(verified.signing_key_id,parsed.key_id,sizeof(parsed.key_id));*output=verified;return AG_MANIFEST_OK;
}
bool ag_manifest_verify_hook(void *context,const uint8_t *bytes,size_t length,ag_ota_plan_t *plan){
    if(plan==NULL)return false;
    ag_manifest_verified_t verified;if(ag_manifest_verify(context,bytes,length,&verified)!=AG_MANIFEST_OK)return false;
    *plan=verified.plan;return true;
}
