/* Host-only fixture codec inspection; no callbacks/flash or transport. */
#include "ota_transfer.h"
#include <stdio.h>
#include <string.h>
static int nibble(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;}
int main(int argc,char **argv){
    if(argc!=3)return 2;
    size_t hex_length=strlen(argv[2]);uint8_t input[AG_OTA_TRANSFER_REQUEST_MAX+1];
    if(hex_length%2||hex_length/2>sizeof(input))return 2;
    for(size_t i=0;i<hex_length/2;++i){int hi=nibble(argv[2][i*2]),lo=nibble(argv[2][i*2+1]);if(hi<0||lo<0)return 2;input[i]=(uint8_t)((hi<<4)|lo);}
    size_t length=hex_length/2;
    if(!strcmp(argv[1],"request")){
        ag_ot_request_t req;
        if(!ag_ot_request_decode(input,length,&req)){puts("INVALID");return 0;}
        uint8_t roundtrip[AG_OTA_TRANSFER_REQUEST_MAX];size_t written=0;
        if(!ag_ot_request_encode(&req,roundtrip,sizeof(roundtrip),&written)||written!=length||memcmp(roundtrip,input,length))return 3;
        printf("%u %u %u ",(unsigned)req.opcode,req.transfer_id,req.offset);
        for(size_t i=0;i<req.body_length;++i)printf("%02x",req.body[i]);
        puts("");return 0;
    }
    if(!strcmp(argv[1],"response")){
        ag_ot_response_t res;
        if(!ag_ot_response_decode(input,length,&res)){puts("INVALID");return 0;}
        uint8_t roundtrip[16];if(!ag_ot_response_encode(&res,roundtrip,sizeof(roundtrip))||memcmp(roundtrip,input,16))return 3;
        printf("%u %u %u %u %u %u\n",(unsigned)res.status,(unsigned)res.phase,res.selection_may_have_changed?1u:0u,res.transfer_id,res.manifest_offset,res.image_offset);return 0;
    }
    return 2;
}
