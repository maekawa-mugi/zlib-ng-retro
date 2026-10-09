/* EE MMI compare64 A/B benchmark. Run on actual PlayStation 2 hardware.
 * Clock resolution may be insufficient: consult EE performance counters
 * before treating these as reliable microbenchmarks. */
#include "zbuild.h"
#include "arch_functions.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "ps2/bench_display.h"

#ifndef MIPS_MMI_COMPARE64
#  error "Build with WITH_MMI_COMPARE64=ON"
#endif

static uint8_t a[288] ALIGNED_(16), b[288] ALIGNED_(16);
static volatile uint32_t sink;

typedef uint32_t (*compare_func)(const uint8_t *, const uint8_t *);

static clock_t run(compare_func compare, const uint8_t *x,
                   const uint8_t *y, unsigned iters) {
    clock_t start = clock();
    uint32_t count = 0;
    for (unsigned i = 0; i < iters; i++)
        count += compare(x, y);
    clock_t stop = clock();
    sink ^= count;
    if (stop == (clock_t)-1 || start == (clock_t)-1)
        return (clock_t)-1;
    return stop - start;
}


#ifdef PS2_SPR_BENCH
/* Real MMI compare256 against two independent 256-byte inputs.
 * Test both hot placement and the total cost of copying to SPR.
 * Names are emitted to stdout, not the limited GS ranking table. */
static clock_t spr_median6(const clock_t samples[6]) {
    clock_t a[6], v; unsigned i,j;
    for (i=0;i<6;i++)a[i]=samples[i];
    for (i=1;i<6;i++){v=a[i];j=i;while(j && a[j-1]>v){
        a[j]=a[j-1];--j;}a[j]=v;}
    return (a[2]+a[3])/2;
}
static int compare_spr_bench(void) {
    static const compare_func kernels[]={
        compare256_mmi_plain,compare256_mmi_prefilter64_swar
    };
    static const char *const names[]={"plain","prefilter64_swar"};
    static const char *const modes[]={
        "ram","spr_a","spr_b","spr_both","spr_xfer"
    };
    static const unsigned mismatches[]={0,15,63,127,255,256};
    static const unsigned offsets[]={0,1,15};
    uint8_t *sa=(uint8_t *)(uintptr_t)0x70000000u;
    uint8_t *sb=(uint8_t *)(uintptr_t)0x70000200u;
    const unsigned iterations=ps2_bench_iterations(30000u);
    unsigned k,oi,mi,v,rep,sample,step;
    printf("ZLIB_SPR_COMPARE_META,R5900,plain_and_prefilter64_swar,6samples\n");
    for(k=0;k<2;k++)for(oi=0;oi<3;oi++)for(mi=0;mi<6;mi++){
        const unsigned off=offsets[oi], mismatch=mismatches[mi];
        const uint8_t *ax=a+off, *bx=b+off;
        clock_t ticks[5][6],med[5];
        uint32_t want;
        for(unsigned i=0;i<256;i++){a[off+i]=b[off+i]=(uint8_t)(i*57u+13u);}
        if(mismatch<256)b[off+mismatch]^=0x80u;
        want=compare256_c(ax,bx);
        if(want!=mismatch)return 1;
        for(v=0;v<5;v++){
            const uint8_t *x=(v==1||v==3)?sa+off:ax;
            const uint8_t *y=(v==2||v==3)?sb+off:bx;
            if(v==1||v==3)memcpy(sa+off,ax,256);
            if(v==2||v==3)memcpy(sb+off,bx,256);
            if(v==4){memcpy(sa+off,ax,256);memcpy(sb+off,bx,256);x=sa+off;y=sb+off;}
            if(kernels[k](x,y)!=want){
                printf("ZLIB_SPR_COMPARE_FAIL,%s,off%u,diff%u,%s\n",
                       names[k],off,mismatch,modes[v]);
                return 1;
            }
        }
        for(sample=0;sample<6;sample++)for(step=0;step<5;step++){
            const unsigned mode=(sample+step)%5;
            const uint8_t *x=(mode==1||mode==3)?sa+off:ax;
            const uint8_t *y=(mode==2||mode==3)?sb+off:bx;
            clock_t begin,end;
            if(mode==1||mode==3)memcpy(sa+off,ax,256);
            if(mode==2||mode==3)memcpy(sb+off,bx,256);
            begin=clock();
            uint32_t result=0;
            for(rep=0;rep<iterations;rep++){
                if(mode==4) {
                    memcpy(sa+off,ax,256);
                    memcpy(sb+off,bx,256);
                    result+=kernels[k](sa+off,sb+off);
                }else result+=kernels[k](x,y);
            }
            end=clock();
            sink^=result;
            if(begin==(clock_t)-1||end==(clock_t)-1||end<=begin){
                puts("ZLIB_SPR_COMPARE_FAIL,clock");return 1;
            }
            ticks[mode][sample]=end-begin;
        }
        for(v=0;v<5;v++){
            med[v]=spr_median6(ticks[v]);
            printf("ZLIB_SPR_COMPARE,%s,off%u,diff%u,%s,%ld,%.4f\n",
                   names[k],off,mismatch,modes[v],(long)med[v],
                   med[v]>0?(double)med[0]/med[v]:0.0);
        }
        if(k==1 && off==0 && mismatch==256 && med[3]>0)
            ps2_bench_spr_highlight(1, (double)med[0]/med[3]);
    }
    puts("ZLIB_SPR_COMPARE_RESULT,PASS,cases=36");
    return 0;
}
#endif

int main(void) {
    static const char *const names[] = {
        "generic", "16-byte", "16-swar", "32-byte", "32-swar", "64-byte", "64-swar"
    };
    ps2_bench_candidates("compare256", names, 7, 1);
    static const unsigned offsets[] = {0, 1, 8, 15};
    static const unsigned mismatches[] = {0, 15, 16, 63, 64, 127, 191, 255, 256};
    for (unsigned i = 0; i < sizeof(a); i++)
        a[i] = b[i] = (uint8_t)(i * 57u + 13u);
    printf("MMI compare256 A/B CLOCKS_PER_SEC=%lu\n",
           (unsigned long)CLOCKS_PER_SEC);
    puts("offset mismatch generic plain swar pre32 pre32swar pre64 pre64swar");

    for (unsigned oi = 0; oi < sizeof(offsets)/sizeof(offsets[0]); oi++) {
        for (unsigned mi = 0; mi < sizeof(mismatches)/sizeof(mismatches[0]); mi++) {
            ps2_bench_case("compare cases", oi * 9 + mi + 1, 36);
                unsigned offset = offsets[oi];
            unsigned mismatch = mismatches[mi];
            uint8_t *x = a + offset, *y = b + offset;
            if (mismatch < 256)
                y[mismatch] ^= 0x80u;
            static const compare_func variants[7] = {
                compare256_c, compare256_mmi_plain, compare256_mmi_swar,
                compare256_mmi_prefilter32, compare256_mmi_prefilter32_swar,
                compare256_mmi_prefilter64, compare256_mmi_prefilter64_swar
            };
            const uint32_t expected = compare256_c(x, y);
            int failed = expected != mismatch;
            for (unsigned v = 1; v < 7; ++v) {
                uint32_t actual = variants[v](x, y);
                ps2_bench_check(v, expected == mismatch && actual == mismatch);
                if (expected != mismatch || actual != mismatch) {
                    printf("MMI compare bench FAIL offset=%u mismatch=%u variant=%u expected=%u actual=%u\n",
                           offset, mismatch, v, (unsigned)expected, (unsigned)actual);
                    failed = 1;
                }
            }
            if (failed) return 1;
            ps2_bench_check(0, expected == mismatch);
            clock_t ticks[7];
            unsigned repetitions = ps2_bench_iterations(100000u);
            /* Reverse every other case to limit systematic warm-cache bias.
             * Dedicated repeated runs remain necessary on real hardware. */
            if ((oi + mi) & 1u) {
                for (int v = 6; v >= 0; --v)
                    ticks[v] = run(variants[v], x, y, repetitions);
            } else {
                for (unsigned v = 0; v < 7; ++v)
                    ticks[v] = run(variants[v], x, y, repetitions);
            }
            for (unsigned v = 0; v < 7; ++v) ps2_bench_ticks(v, ticks[v]);
            if (ticks[0] <= 0 || ticks[1] <= 0 || ticks[2] <= 0 ||
                ticks[3] <= 0 || ticks[4] <= 0 || ticks[5] <= 0 || ticks[6] <= 0) {
                printf("%u %u clock_unavailable_or_too_coarse\n", offset, mismatch);
            } else {
                printf("%u %u", offset, mismatch);
                for (unsigned v = 0; v < 7; ++v) printf(" %ld", (long)ticks[v]);
                putchar('\n');
            }
            if (mismatch < 256)
                y[mismatch] ^= 0x80u;
        }
    }
#ifdef PS2_SPR_BENCH
    if (compare_spr_bench()) return 1;
#endif
    printf("benchmark sink=%lu\n", (unsigned long)sink);
    return 0;
}
