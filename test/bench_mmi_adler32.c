/* A/B benchmark for PS2 EE Adler-32. No platform-specific timers required.
 * clock() is an approximation; use EE performance counters for final results.
 * Not registered with ctest; run the executable on actual PS2 hardware. */
#include "zbuild.h"
#include "arch_functions.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "ps2/bench_display.h"

#ifndef MIPS_MMI_ADLER32
#  error "Build with WITH_MMI_ADLER32=ON"
#endif

static uint8_t source[65536 + 16] ALIGNED_(16);
static volatile uint32_t sink;

typedef uint32_t (*adler_func)(uint32_t, const uint8_t *, size_t);

static clock_t bench(adler_func fn, const uint8_t *buf,
                     size_t len, unsigned iterations) {
    clock_t begin = clock();
    uint32_t check = 0;
    for (unsigned i = 0; i < iterations; ++i)
        check ^= fn(1u, buf, len) + i;
    clock_t end = clock();
    sink ^= check;
    if (begin == (clock_t)-1 || end == (clock_t)-1)
        return (clock_t)-1;
    return end - begin;
}


#ifdef PS2_SPR_BENCH
/* Test placement for real EE MMI Adler32 checksum kernels.
 * Input-only, so compare preloaded SPR against transfer-inclusive SPR. */
static clock_t spr_median6_adler(const clock_t samples[6]) {
    clock_t a[6],v; unsigned i,j;
    for(i=0;i<6;i++)a[i]=samples[i];
    for(i=1;i<6;i++){v=a[i];j=i;while(j&&a[j-1]>v){
        a[j]=a[j-1];--j;}a[j]=v;}
    return (a[2]+a[3])/2;
}
static int spr_bench_adler(void) {
    static const size_t sizes[]={64,1024,8192,16368};
    static const unsigned iter[]={30000,3000,600,200};
    static const adler_func funcs[]={
        adler32_mmi_prefix,adler32_mmi_formula
    };
    static const char *const names[]={"mmi_prefix","mmi_formula"};
    static const char *const modes[]={"ram","spr_hot","spr_copy"};
    uint8_t *spr=(uint8_t *)(uintptr_t)0x70000000u;
    unsigned k,z,off,sample,step,mode,rep;
    puts("ZLIB_SPR_ADLER_META,R5900,6samples,3placements");
    for(k=0;k<2;k++)for(z=0;z<4;z++)for(off=0;off<2;off++) {
        size_t size=sizes[z];
        const uint8_t *p=source+off;
        uint32_t expected=adler32_c(1u,p,size);
        clock_t timings[3][6], med[3];
        unsigned count=ps2_bench_iterations(iter[z]);
        memcpy(spr+off,p,size);
        for(mode=0;mode<3;mode++) {
            const uint8_t *buf=mode?spr+off:p;
            if(funcs[k](1u,buf,size)!=expected) {
                printf("ZLIB_SPR_ADLER_FAIL,%s,%lu,%u,%s\n",
                       names[k],(unsigned long)size,off,modes[mode]);
                return 1;
            }
        }
        for(sample=0;sample<6;sample++)for(step=0;step<3;step++) {
            mode=(sample+step)%3;
            if(mode==1)memcpy(spr+off,p,size);
            clock_t begin=clock();
            uint32_t value=0;
            for(rep=0;rep<count;rep++) {
                if(mode==2)memcpy(spr+off,p,size);
                value^=funcs[k](1u,mode?spr+off:p,size)+rep;
            }
            clock_t end=clock();
            sink^=value;
            if(begin==(clock_t)-1||end==(clock_t)-1||end<=begin) {
                puts("ZLIB_SPR_ADLER_FAIL,clock");return 1;
            }
            timings[mode][sample]=end-begin;
        }
        for(mode=0;mode<3;mode++) {
            med[mode]=spr_median6_adler(timings[mode]);
            printf("ZLIB_SPR_ADLER,%s,%lu,%u,%s,%ld,%.4f\n",
                   names[k],(unsigned long)size,off,modes[mode],
                   (long)med[mode],(double)med[0]/med[mode]);
        }
    }
    puts("ZLIB_SPR_ADLER_RESULT,PASS,cases=16");
    return 0;
}
#endif

int main(void) {
    static const char *const names[] = {"generic", "prefix", "formula"};
    ps2_bench_candidates("adler32", names, 3, 1);
    static const size_t lengths[] = {64, 1024, 8192, 65536};
    static const unsigned iterations[] = {200000, 20000, 2000, 200};
    uint32_t rng = 0x9e3779b9u;

    for (unsigned i = 0; i < sizeof(source); i++) {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        source[i] = (uint8_t)rng;
    }
    printf("MMI Adler-32 A/B, CLOCKS_PER_SEC=%lu\n",
           (unsigned long)CLOCKS_PER_SEC);
    puts("length offset iterations C_ticks prefix_ticks formula_ticks C_over_prefix C_over_formula");
    for (unsigned index = 0; index < sizeof(lengths)/sizeof(lengths[0]); ++index) {
        for (unsigned offset = 0; offset < 2; ++offset) {
            ps2_bench_case("Adler cases", index * 2 + offset + 1, 8);
                const uint8_t *p = source + offset;
            size_t n = lengths[index];
            unsigned count = ps2_bench_iterations(iterations[index]);
            uint32_t expected = adler32_c(1u, p, n);
            int ok_a = expected == adler32_mmi_prefix(1u, p, n);
            int ok_b = expected == adler32_mmi_formula(1u, p, n);
            ps2_bench_check(0, 1);
            ps2_bench_check(1, ok_a);
            ps2_bench_check(2, ok_b);
            if (!ok_a || !ok_b) {
                printf("checksum mismatch len=%lu offset=%u\n",
                       (unsigned long)n, offset);
                return 1;
            }
            /* Alternate order to reduce cache/warmup bias. */
            clock_t tc, tp, tf;
            if ((index + offset) & 1u) {
                tf = bench(adler32_mmi_formula, p, n, count);
                tp = bench(adler32_mmi_prefix, p, n, count);
                tc = bench(adler32_c, p, n, count);
            } else {
                tc = bench(adler32_c, p, n, count);
                tp = bench(adler32_mmi_prefix, p, n, count);
                tf = bench(adler32_mmi_formula, p, n, count);
            }
            ps2_bench_ticks(0, tc);
            ps2_bench_ticks(1, tp);
            ps2_bench_ticks(2, tf);
            if (tc <= 0 || tp <= 0 || tf <= 0) {
                printf("%5lu %2u %8u clock_unavailable_or_too_coarse\n",
                       (unsigned long)n, offset, count);
                continue;
            }
            printf("%5lu %2u %8u %ld %ld %ld %.3f %.3f\n",
                   (unsigned long)n, offset, count,
                   (long)tc, (long)tp, (long)tf,
                   (double)tc / (double)tp, (double)tc / (double)tf);
        }
    }
#ifdef PS2_SPR_BENCH
    if (spr_bench_adler()) return 1;
#endif
    printf("benchmark sink=%lu\n", (unsigned long)sink);
    return 0;
}
