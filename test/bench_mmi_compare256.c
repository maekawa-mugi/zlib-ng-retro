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
    puts("offset mismatch generic_ticks plain_byte plain_swar prefilter_byte prefilter_swar C_over_plainbyte C_over_plainswar C_over_prefbyte C_over_prefswar");

    for (unsigned oi = 0; oi < sizeof(offsets)/sizeof(offsets[0]); oi++) {
        for (unsigned mi = 0; mi < sizeof(mismatches)/sizeof(mismatches[0]); mi++) {
            unsigned offset = offsets[oi];
            unsigned mismatch = mismatches[mi];
            uint8_t *x = a + offset, *y = b + offset;
            if (mismatch < 256)
                y[mismatch] ^= 0x80u;
            static const compare_func variants[5] = {
                compare256_c, compare256_mmi_plain,
                compare256_mmi_swar, compare256_mmi_prefilter64,
                compare256_mmi_prefilter64_swar
            };
            const uint32_t expected = compare256_c(x, y);
            for (unsigned v = 1; v < 5; ++v) {
                uint32_t actual = variants[v](x, y);
                if (expected != mismatch || actual != mismatch) {
                    printf("MMI compare bench FAIL offset=%u mismatch=%u variant=%u expected=%u actual=%u\n",
                           offset, mismatch, v, expected, actual);
                    return 1;
                }
            }
            clock_t ticks[5];
            /* Reverse every other case to limit systematic warm-cache bias.
             * Dedicated repeated runs remain necessary on real hardware. */
            if ((oi + mi) & 1u) {
                for (int v = 4; v >= 0; --v)
                    ticks[v] = run(variants[v], x, y, 100000u);
            } else {
                for (unsigned v = 0; v < 5; ++v)
                    ticks[v] = run(variants[v], x, y, 100000u);
            }
            if (ticks[0] <= 0 || ticks[1] <= 0 || ticks[2] <= 0 ||
                ticks[3] <= 0 || ticks[4] <= 0) {
                printf("%u %u clock_unavailable_or_too_coarse\n", offset, mismatch);
            } else {
                printf("%u %u %ld %ld %ld %ld %ld %.3f %.3f %.3f %.3f\n",
                       offset, mismatch, (long)ticks[0], (long)ticks[1],
                       (long)ticks[2], (long)ticks[3], (long)ticks[4],
                       (double)ticks[0]/(double)ticks[1],
                       (double)ticks[0]/(double)ticks[2],
                       (double)ticks[0]/(double)ticks[3],
                       (double)ticks[0]/(double)ticks[4]);
            }
            if (mismatch < 256)
                y[mismatch] ^= 0x80u;
        }
    }
    printf("benchmark sink=%lu\n", (unsigned long)sink);
    return 0;
}
