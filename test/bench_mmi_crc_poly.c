/* PS2 EE: size-resolved CRC32 competition, strict correctness gates.
 * Real polynomial zero-scatter R5900 MMI against conventional CRC32,
 * existing braid, and original Chorba single/paired. No SPR or DMA.
 *
 * Per-size eligibility is mandatory: a fallback cannot "win" as
 * an accelerated polynomial. Winner records include all timing samples.
 */
#include "zbuild.h"
#include "arch_functions.h"
#include "ps2/bench_display.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>

extern unsigned ps2_crc_poly_count(void);
extern const char *ps2_crc_poly_name(unsigned);
extern size_t ps2_crc_poly_minimum(unsigned);
extern unsigned ps2_crc_poly_degree(unsigned);
extern unsigned ps2_crc_poly_terms(unsigned);
extern uint32_t ps2_crc_poly_mmi(unsigned,uint32_t,const uint8_t *,size_t);

enum { POLY_COUNT = 8, CANDIDATES = 13, SAMPLES = 6 };
/* 0=braid, 1=byte-table, 2=reference bitwise, 3/4=original EE Chorba,
 * 5..12=paper polynomial MMI candidates. */
static uint8_t data[4u*1024u*1024u+64u] ALIGNED_(16);
static uint32_t byte_table[256];
static volatile uint32_t anti_dce;
static const char *const candidate_names[CANDIDATES] = {
    "braid","table256","bitwise","original_single","original_paired",
    "gen32","chorba352","small300","small600",
    "sparse4_3006","dense4_5869","dense5_14870","sparse3_91639"
};
static uint32_t ordinary_crc_table(uint32_t crc,const uint8_t *p,size_t n)
{
    crc=~crc;
    for(size_t i=0;i<n;i++)
        crc=(crc>>8)^byte_table[(crc^p[i])&255u];
    return ~crc;
}
static uint32_t ordinary_crc_bitwise(uint32_t crc,const uint8_t *p,size_t n)
{
    crc=~crc;
    for(size_t i=0;i<n;i++){
        crc^=p[i];
        for(unsigned k=0;k<8;k++)
            crc=(crc>>1)^((crc&1u)?0xedb88320u:0u);
    }
    return ~crc;
}
static uint32_t invoke(unsigned v,uint32_t crc,const uint8_t *p,size_t n)
{
    if(v==0u)return crc32_braid(crc,p,n);
    if(v==1u)return ordinary_crc_table(crc,p,n);
    if(v==2u)return ordinary_crc_bitwise(crc,p,n);
    if(v==3u)return crc32_chorba_mmi_single(crc,p,n);
    if(v==4u)return crc32_chorba_mmi_paired(crc,p,n);
    return ps2_crc_poly_mmi(v-5u,crc,p,n);
}
static int eligible(unsigned v,size_t n)
{
    if(v==2u)return n<=8192u;
    if(v==3u || v==4u)return n>=4096u;
    if(v>=5u)return n>=ps2_crc_poly_minimum(v-5u);
    return 1;
}
static clock_t timed(unsigned v, uint32_t seed,const uint8_t *p,size_t n,
                     unsigned reps)
{
    volatile uint32_t tmp=0;
    clock_t start=clock();
    for(unsigned i=0;i<reps;i++)
        tmp^=invoke(v,seed+i,p,n)+(uint32_t)i;
    clock_t stop=clock();
    anti_dce^=tmp;
    if(start==(clock_t)-1 || stop==(clock_t)-1 || stop<=start)
        return (clock_t)-1;
    return stop-start;
}
static double median(const double *v)
{
    double a[SAMPLES],x;
    for(unsigned i=0;i<SAMPLES;i++)a[i]=v[i];
    for(unsigned i=1;i<SAMPLES;i++){
        x=a[i];unsigned j=i;
        while(j>0 && a[j-1]>x){a[j]=a[j-1];--j;}
        a[j]=x;
    }
    return (a[2]+a[3])*0.5;
}
static void init(void)
{
    for(unsigned v=0;v<256;v++){
        uint32_t crc=v;
        for(unsigned i=0;i<8;i++)
            crc=(crc>>1)^((crc&1)?0xedb88320u:0u);
        byte_table[v]=crc;
    }
    uint32_t seed=0x811c9dc5u;
    for(size_t i=0;i<sizeof(data);i++){
        seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
        data[i]=(uint8_t)seed;
    }
}
int main(void)
{
    static const size_t sizes[]={
        128u,1024u,4096u,8192u,16384u,32768u,
        65536u,262144u,1048576u,4194304u
    };
    static const char *const size_labels[]={
        "128B","1K","4K","8K","16K","32K","64K","256K","1M","4M"
    };
    if(ps2_crc_poly_count()!=POLY_COUNT){
        puts("CRC_POLY_FAIL,variant_count");return 1;
    }
    init();
    const uint32_t seed=0x12345678u;
    if(ordinary_crc_table(0,(const uint8_t*)"123456789",9)!=0xcbf43926u ||
       ordinary_crc_bitwise(0,(const uint8_t*)"123456789",9)!=0xcbf43926u){
        puts("CRC_POLY_FAIL,ordinary_crc_reference");return 1;
    }
    printf("CRC_POLY_META,EE_R5900,reflected_0x1DB710641,polys=%u,ordinary=3,original=2,samples=%u\n",
           ps2_crc_poly_count(),SAMPLES);
    for(unsigned v=0;v<ps2_crc_poly_count();v++)
        printf("CRC_POLY_SPEC,%s,%u,%u,%lu\n",
            ps2_crc_poly_name(v),ps2_crc_poly_degree(v),
            ps2_crc_poly_terms(v),(unsigned long)ps2_crc_poly_minimum(v));
    unsigned checks=0, winners=0, timer_na=0;
    for(unsigned si=0;si<sizeof(sizes)/sizeof(sizes[0]);si++)
       for(unsigned offset=0;offset<2;offset++){
        const size_t n=sizes[si];
        const uint8_t *p=data+offset;
        ps2_bench_case("CRC polynomial",si*2u+offset+1u,20u);
        const uint32_t expected=crc32_braid(seed,p,n);
        int valid[CANDIDATES]={0};
        double ticks[CANDIDATES]={0};
        double raw[CANDIDATES][SAMPLES]={{0}};
        for(unsigned v=0;v<CANDIDATES;v++){
            if(!eligible(v,n)){
                printf("CRC_POLY_CASE,%s,%u,%s,SKIP_BELOW_MINIMUM\n",
                       size_labels[si],offset,candidate_names[v]);
                continue;
            }
            uint32_t got=invoke(v,seed,p,n);
            ++checks;
            if(got!=expected){
                printf("CRC_POLY_FAIL,%s,%u,%s,expected=%08lx,got=%08lx\n",
                     size_labels[si],offset,candidate_names[v],
                     (unsigned long)expected,(unsigned long)got);
                return 1;
            }
            valid[v]=1;
        }
        /* Identical repetitions for all candidates at this length;
         * rotate the order across samples to reduce temperature bias. */
        unsigned reps=ps2_bench_iterations(120u);
        if(reps==0)reps=1;
        unsigned cap=n<=4096u?32u:n<=32768u?8u:1u;
        if(reps>cap)reps=cap;
        for(unsigned sample=0;sample<SAMPLES;sample++)
            for(unsigned step=0;step<CANDIDATES;step++){
                unsigned v=(sample+step+offset)%CANDIDATES;
                if(!valid[v])continue;
                clock_t t=timed(v,seed,p,n,reps);
                raw[v][sample]=t<=0?0.0:(double)t/(double)reps;
                printf("CRC_POLY_SAMPLE,%s,%u,%s,%u,%lu,%u\n",
                    size_labels[si],offset,candidate_names[v],sample,
                    t<=0?0ul:(unsigned long)t,reps);
            }
        int best=-1;
        for(unsigned v=0;v<CANDIDATES;v++){
            if(!valid[v])continue;
            int good=1;
            for(unsigned sample=0;sample<SAMPLES;sample++)
                if(raw[v][sample]<=0.0)good=0;
            if(!good){
                ++timer_na;
                printf("CRC_POLY_CASE,%s,%u,%s,TIMER_NA\n",
                       size_labels[si],offset,candidate_names[v]);
                continue;
            }
            ticks[v]=median(raw[v]);
            if(best<0 || ticks[v]<ticks[best])best=(int)v;
            printf("CRC_POLY_CASE,%s,%u,%s,PASS,ticks_per_call=%.3f,reps=%u\n",
                   size_labels[si],offset,candidate_names[v],ticks[v],reps);
        }
        if(best<0){
            printf("CRC_POLY_WINNER,%s,%u,UNDETERMINED\n",size_labels[si],offset);
            continue;
        }
        double speed=ticks[best]>0? ticks[0]/ticks[best]:0.0;
        printf("CRC_POLY_WINNER,%s,%u,%s,%.5f\n",
               size_labels[si],offset,candidate_names[best],speed);
        ++winners;
        if(offset==0 && (si==2 || si==6 || si==8))
            ps2_bench_poly_highlight(si==2?0u:si==6?1u:2u,
                                    candidate_names[best],speed);
       }
    printf("CRC_POLY_RESULT,PASS,checks=%u,winners=%u,timer_na=%u,sink=%lu\n",
           checks,winners,timer_na,(unsigned long)anti_dce);
    fflush(stdout);
    return 0;
}
