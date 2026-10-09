/* Native host differential test for every published zero-polynomial
 * CRC32 candidate. Run with CRC_POLY_HOST_TEST, never dereference SPR.
 * Normal -O2/-O3 GCC tests exercise scatter, residue and inverse shifts.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
uint32_t ps2_crc_poly_mmi(unsigned, uint32_t, const uint8_t *,size_t);
unsigned ps2_crc_poly_count(void);
const char *ps2_crc_poly_name(unsigned);
size_t ps2_crc_poly_minimum(unsigned);
unsigned ps2_crc_poly_degree(unsigned);
unsigned ps2_crc_poly_terms(unsigned);

static uint32_t tbl[256];static int ready;
uint32_t crc32_braid(uint32_t crc, const uint8_t *data,size_t len)
{
    if (!ready) {
        for(unsigned i=0;i<256;i++){
            uint32_t v=i;
            for(unsigned k=0;k<8;k++)
                v=(v>>1)^((v&1)?0xedb88320u:0u);
            tbl[i]=v;
        }
        ready=1;
    }
    crc=~crc;
    for(size_t i=0;i<len;i++)crc=tbl[(crc^data[i])&255u]^(crc>>8);
    return ~crc;
}
static uint32_t rng=0x8911b27fu;
static uint32_t random_u32(void)
{
    rng^=rng<<13; rng^=rng>>17; rng^=rng<<5;return rng;
}
static uint8_t input[4u*1024u*1024u+256u];
static uint8_t saved[4u*1024u*1024u+256u];
int main(void)
{
    if(crc32_braid(0u,(const uint8_t *)"123456789",9u)!=0xcbf43926u){
        puts("CRC_POLY_HOST_FAIL,bitwise_reference");return 1;
    }
    for(size_t i=0;i<sizeof(input);i++)input[i]=(uint8_t)random_u32();
    memcpy(saved,input,sizeof(input));
    unsigned total=0;
    for(unsigned p=0;p<ps2_crc_poly_count();p++){
        size_t threshold=ps2_crc_poly_minimum(p);
        size_t lens[]={0,1,15,16,1023,4096,threshold-1,
                       threshold,threshold+1,threshold+31};
        printf("CRC_POLY_HOST_META,%s,%u,%u,%lu\n",
               ps2_crc_poly_name(p),ps2_crc_poly_degree(p),
               ps2_crc_poly_terms(p),(unsigned long)threshold);
        for(unsigned v=0;v<sizeof(lens)/sizeof(lens[0]);v++)
            for(unsigned off=0;off<2;off++)
                for(unsigned seedid=0;seedid<3;seedid++){
                    size_t len=lens[v];
                    if(len+off>sizeof(input))continue;
                    uint32_t seed=seedid==0?0u:
                                  seedid==1?0xffffffffu:0x12345678u;
                    uint32_t expected=crc32_braid(seed,input+off,len);
                    uint32_t got=ps2_crc_poly_mmi(p,seed,input+off,len);
                    if(got!=expected||memcmp(input,saved,sizeof(input))){
                        printf("CRC_POLY_HOST_FAIL,%s,%lu,%u,%08lx,%08lx\n",
                               ps2_crc_poly_name(p),(unsigned long)len,off,
                               (unsigned long)expected,(unsigned long)got);
                        return 1;
                    }
                    ++total;
                }
        puts("CRC_POLY_HOST_CANDIDATE,PASS");
    }
    printf("CRC_POLY_HOST_RESULT,PASS,cases=%u\n",total);
    return 0;
}
