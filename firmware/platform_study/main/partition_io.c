#include "partition_io.h"
#include "esp_flash.h"
#include <string.h>
#define SECTOR 4096u
#define CHECK_CHUNK 256u
#define MAX_PARTITIONS 128u
static bool label(const char value[17]){
    if(memchr(value,0,17)==NULL||value[0]==0)return false;
    for(size_t i=0;value[i];++i){unsigned char c=(unsigned char)value[i];
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;
    }
    return true;
}
static bool identity(const esp_partition_t *a,const esp_partition_t *b){
    return a!=NULL&&b!=NULL&&a->flash_chip==b->flash_chip&&a->type==b->type&&a->subtype==b->subtype
        &&a->address==b->address&&a->size==b->size&&a->erase_size==b->erase_size
        &&a->encrypted==b->encrypted&&a->readonly==b->readonly&&memcmp(a->label,b->label,17)==0;
}
static bool extent(const esp_partition_t *p){return p!=NULL&&p->size>0&&(uint64_t)p->address+p->size<=UINT32_MAX;}
static bool overlap(const esp_partition_t *a,const esp_partition_t *b){
    return a->flash_chip==b->flash_chip&&(uint64_t)a->address<(uint64_t)b->address+b->size
        &&(uint64_t)b->address<(uint64_t)a->address+a->size;
}
static bool selected(const esp_partition_t *p,const char name[17],uint8_t subtype,uint32_t size){
    return extent(p)&&p->flash_chip==esp_flash_default_chip&&p->flash_chip!=NULL&&p->type==ESP_PARTITION_TYPE_DATA
        &&p->subtype==subtype&&strcmp(p->label,name)==0&&p->size==size&&p->address%SECTOR==0
        &&p->erase_size==SECTOR&&!p->encrypted&&!p->readonly&&identity(p,esp_partition_verify(p));
}
static bool discover(const ag_partition_layout_t *layout,const esp_partition_t **config,const esp_partition_t **history){
    uint32_t flash_bytes=0;
    if(esp_flash_default_chip==NULL||esp_flash_get_size(esp_flash_default_chip,&flash_bytes)!=ESP_OK||flash_bytes==0)return false;
    esp_partition_iterator_t it=NULL;unsigned count=0,configs=0,histories=0;
    if(esp_partition_find_err(ESP_PARTITION_TYPE_ANY,ESP_PARTITION_SUBTYPE_ANY,NULL,&it)!=ESP_OK||it==NULL)return false;
    while(it!=NULL){const esp_partition_t *p=esp_partition_get(it);
        if(p==NULL||++count>MAX_PARTITIONS||memchr(p->label,0,sizeof(p->label))==NULL){esp_partition_iterator_release(it);return false;}
        if(strcmp(p->label,layout->config_label)==0){++configs;*config=p;}
        if(strcmp(p->label,layout->history_label)==0){++histories;*history=p;}
        it=esp_partition_next(it);
    }
    if(configs!=1||histories!=1
        ||(uint64_t)(*config)->address+(*config)->size>flash_bytes
        ||(uint64_t)(*history)->address+(*history)->size>flash_bytes
        ||!selected(*config,layout->config_label,layout->config_subtype,AG_CONFIG_STORE_BYTES)
        ||!selected(*history,layout->history_label,layout->history_subtype,(uint32_t)layout->history_sectors*SECTOR))return false;
    if(esp_partition_find_err(ESP_PARTITION_TYPE_ANY,ESP_PARTITION_SUBTYPE_ANY,NULL,&it)!=ESP_OK||it==NULL)return false;
    count=0;
    while(it!=NULL){const esp_partition_t *p=esp_partition_get(it);
        if(p==NULL||++count>MAX_PARTITIONS||!extent(p)
            ||(p!=*config&&overlap(*config,p))||(p!=*history&&overlap(*history,p))){esp_partition_iterator_release(it);return false;}
        it=esp_partition_next(it);
    }
    return true;
}
static bool valid_partition(ag_partition_store_t *s,const esp_partition_t *p){
    const esp_partition_t *expected=p==s->config?&s->config_identity:&s->history_identity;
    if(!identity(p,expected)||!identity(p,esp_partition_verify(p))){s->write_fault=true;return false;}
    return true;
}
static bool bounds(const esp_partition_t *p,uint32_t offset,size_t length){return length>0&&offset<=p->size&&length<=p->size-offset;}
static bool enter(ag_partition_store_t *s){if(s==NULL||!s->initialized||s->busy)return false;s->busy=true;return true;}
static size_t read_bytes(ag_partition_store_t *s,const esp_partition_t *p,uint32_t offset,uint8_t *out,size_t length){
    if(out==NULL||!enter(s))return 0;
    size_t result=0;
    if(bounds(p,offset,length)&&valid_partition(s,p)){
        if(esp_partition_read(p,offset,out,length)==ESP_OK)result=length;else s->write_fault=true;
    }
    s->busy=false;return result;
}
static size_t program_bytes(ag_partition_store_t *s,const esp_partition_t *p,uint32_t offset,const uint8_t *bytes,size_t length){
    if(bytes==NULL||!enter(s))return 0;
    size_t result=0;uint8_t verify[CHECK_CHUNK];
    if(s->write_fault||!bounds(p,offset,length)||!valid_partition(s,p))goto done;
    for(size_t at=0;at<length;){size_t n=length-at;if(n>sizeof(verify))n=sizeof(verify);
        if(esp_partition_read(p,offset+at,verify,n)!=ESP_OK){s->write_fault=true;goto done;}
        for(size_t i=0;i<n;++i)if((verify[i]&bytes[at+i])!=bytes[at+i]){s->write_fault=true;goto done;}
        at+=n;
    }
    /* A failed SDK write may already have programmed a prefix. Never retry. */
    if(esp_partition_write(p,offset,bytes,length)!=ESP_OK){s->write_fault=true;goto done;}
    for(size_t at=0;at<length;){size_t n=length-at;if(n>sizeof(verify))n=sizeof(verify);
        if(esp_partition_read(p,offset+at,verify,n)!=ESP_OK||memcmp(verify,bytes+at,n)!=0){s->write_fault=true;goto done;}
        at+=n;
    }
    result=length;
 done:s->busy=false;return result;
}
static bool erase(ag_partition_store_t *s,const esp_partition_t *p,uint32_t offset,size_t length){
    if(!enter(s))return false;
    bool result=false;uint8_t verify[CHECK_CHUNK];
    if(s->write_fault||length!=SECTOR||offset%SECTOR||!bounds(p,offset,length)||!valid_partition(s,p))goto done;
    if(esp_partition_erase_range(p,offset,length)!=ESP_OK){s->write_fault=true;goto done;}
    for(size_t at=0;at<length;at+=sizeof(verify)){
        if(esp_partition_read(p,offset+at,verify,sizeof(verify))!=ESP_OK){s->write_fault=true;goto done;}
        for(size_t i=0;i<sizeof(verify);++i)if(verify[i]!=0xff){s->write_fault=true;goto done;}
    }
    result=true;
 done:s->busy=false;return result;
}
static size_t config_read(void *context,uint32_t offset,uint8_t *out,size_t length){ag_partition_store_t *s=context;return s?read_bytes(s,s->config,offset,out,length):0;}
static size_t config_program(void *context,uint32_t offset,const uint8_t *bytes,size_t length){ag_partition_store_t *s=context;return s?program_bytes(s,s->config,offset,bytes,length):0;}
static bool config_erase(void *context,uint32_t offset,size_t length){ag_partition_store_t *s=context;return s&&erase(s,s->config,offset,length);}
static size_t history_read(void *context,uint32_t offset,uint8_t *out,size_t length){ag_partition_store_t *s=context;return s?read_bytes(s,s->history,offset,out,length):0;}
static size_t history_program(void *context,uint32_t offset,const uint8_t *bytes,size_t length){ag_partition_store_t *s=context;return s?program_bytes(s,s->history,offset,bytes,length):0;}
static bool history_erase(void *context,uint32_t offset){ag_partition_store_t *s=context;return s&&erase(s,s->history,offset,SECTOR);}
bool ag_partition_store_bind(ag_partition_store_t *s,const ag_partition_layout_t *layout){
    if(s==NULL||layout==NULL||s->initialized||s->busy||!label(layout->config_label)||!label(layout->history_label)
        ||strcmp(layout->config_label,layout->history_label)==0||layout->config_subtype<0x40||layout->history_subtype<0x40
        ||layout->config_subtype==layout->history_subtype||layout->history_sectors<2||layout->history_sectors>AG_JOURNAL_MAX_SECTORS)return false;
    ag_partition_layout_t copy=*layout;const esp_partition_t *config=NULL,*history=NULL;
    s->busy=true;bool found=discover(&copy,&config,&history);s->busy=false;if(!found)return false;
    memset(s,0,sizeof(*s));s->config=config;s->history=history;s->config_identity=*config;s->history_identity=*history;
    s->config_io=(ag_config_flash_io_t){s,config_read,config_program,config_erase};
    s->history_io=(ag_journal_io_t){s,history_read,history_program,history_erase};s->initialized=true;return true;
}
