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



int main(void) {
    static const char *const names[] = {
        "generic", "16-byte", "16-swar", "32-byte", "32-swar",
        "64-byte", "64-swar", "hybrid16", "hybrid16-64"
    };
    ps2_bench_candidates("compare256", names, 9, 1);
    static const unsigned offsets[] = {0, 1, 8, 15};
    static const unsigned mismatches[] = {0, 15, 16, 63, 64, 127, 191, 255, 256};
    for (unsigned i = 0; i < sizeof(a); i++)
        a[i] = b[i] = (uint8_t)(i * 57u + 13u);
    printf("MMI compare256 A/B CLOCKS_PER_SEC=%lu\n",
           (unsigned long)CLOCKS_PER_SEC);
    puts("offset mismatch generic plain swar pre32 pre32swar pre64 pre64swar hybrid16 hybrid16-64");

    for (unsigned oi = 0; oi < sizeof(offsets)/sizeof(offsets[0]); oi++) {
        for (unsigned mi = 0; mi < sizeof(mismatches)/sizeof(mismatches[0]); mi++) {
            ps2_bench_case("compare cases", oi * 9 + mi + 1, 36);
                unsigned offset = offsets[oi];
            unsigned mismatch = mismatches[mi];
            uint8_t *x = a + offset, *y = b + offset;
            if (mismatch < 256)
                y[mismatch] ^= 0x80u;
            static const compare_func variants[9] = {
                compare256_c, compare256_mmi_plain, compare256_mmi_swar,
                compare256_mmi_prefilter32, compare256_mmi_prefilter32_swar,
                compare256_mmi_prefilter64, compare256_mmi_prefilter64_swar,
                compare256_mmi_hybrid16, compare256_mmi_hybrid16_pre64
            };
            const uint32_t expected = compare256_c(x, y);
            int failed = expected != mismatch;
            for (unsigned v = 1; v < 9; ++v) {
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
            clock_t ticks[9];
            unsigned repetitions = ps2_bench_iterations(100000u);
            /* Reverse every other case to limit systematic warm-cache bias.
             * Dedicated repeated runs remain necessary on real hardware. */
            if ((oi + mi) & 1u) {
                for (int v = 8; v >= 0; --v)
                    ticks[v] = run(variants[v], x, y, repetitions);
            } else {
                for (unsigned v = 0; v < 9; ++v)
                    ticks[v] = run(variants[v], x, y, repetitions);
            }
            for (unsigned v = 0; v < 9; ++v) ps2_bench_ticks(v, ticks[v]);
            int valid=1;
            for(unsigned v=0;v<9;++v)
                if(ticks[v]<=0) valid=0;
            if (!valid) {
                printf("%u %u clock_unavailable_or_too_coarse\n", offset, mismatch);
            } else {
                printf("%u %u", offset, mismatch);
                for (unsigned v = 0; v < 9; ++v) printf(" %ld", (long)ticks[v]);
                putchar('\n');
            }
            if (mismatch < 256)
                y[mismatch] ^= 0x80u;
        }
    }
    printf("benchmark sink=%lu\n", (unsigned long)sink);
    return 0;
}
