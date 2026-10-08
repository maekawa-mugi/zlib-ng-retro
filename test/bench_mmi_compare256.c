/* EE MMI compare64 A/B benchmark. Run on actual PlayStation 2 hardware.
 * Clock resolution may be insufficient: consult EE performance counters
 * before treating these as reliable microbenchmarks. */
#include "zbuild.h"
#include "arch_functions.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>

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
    static const unsigned offsets[] = {0, 1, 8, 15};
    static const unsigned mismatches[] = {0, 15, 16, 63, 64, 127, 191, 255, 256};
    for (unsigned i = 0; i < sizeof(a); i++)
        a[i] = b[i] = (uint8_t)(i * 57u + 13u);
    printf("MMI compare256 A/B CLOCKS_PER_SEC=%lu\n",
           (unsigned long)CLOCKS_PER_SEC);
    puts("offset mismatch generic_ticks MMI_ticks ratio_generic_over_MMI");

    for (unsigned oi = 0; oi < sizeof(offsets)/sizeof(offsets[0]); oi++) {
        for (unsigned mi = 0; mi < sizeof(mismatches)/sizeof(mismatches[0]); mi++) {
            unsigned offset = offsets[oi];
            unsigned mismatch = mismatches[mi];
            uint8_t *x = a + offset, *y = b + offset;
            if (mismatch < 256)
                y[mismatch] ^= 0x80u;
            uint32_t expected = compare256_c(x, y);
            uint32_t got = compare256_mmi(x, y);
            if (expected != mismatch || got != mismatch) {
                printf("MMI compare benchmark FAIL offset=%u mismatch=%u generic=%u mmi=%u\n",
                       offset, mismatch, expected, got);
                return 1;
            }
            clock_t tgeneric, tmmi;
            if ((oi + mi) & 1u) {
                tmmi = run(compare256_mmi, x, y, 100000u);
                tgeneric = run(compare256_c, x, y, 100000u);
            } else {
                tgeneric = run(compare256_c, x, y, 100000u);
                tmmi = run(compare256_mmi, x, y, 100000u);
            }
            if (tgeneric <= 0 || tmmi <= 0) {
                printf("%u %u clock_unavailable_or_too_coarse\n", offset, mismatch);
            } else {
                printf("%u %u %ld %ld %.3f\n", offset, mismatch,
                       (long)tgeneric, (long)tmmi,
                       (double)tgeneric / (double)tmmi);
            }
            if (mismatch < 256)
                y[mismatch] ^= 0x80u;
        }
    }
    printf("benchmark sink=%lu\n", (unsigned long)sink);
    return 0;
}
