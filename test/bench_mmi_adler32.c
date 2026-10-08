/* A/B benchmark for PS2 EE Adler-32. No platform-specific timers required.
 * clock() is an approximation; use EE performance counters for final results.
 * Not registered with ctest; run the executable on actual PS2 hardware. */
#include "zbuild.h"
#include "arch_functions.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>

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

int main(void) {
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
            const uint8_t *p = source + offset;
            size_t n = lengths[index];
            unsigned count = iterations[index];
            uint32_t expected = adler32_c(1u, p, n);
            if (expected != adler32_mmi_prefix(1u, p, n) ||
                expected != adler32_mmi_formula(1u, p, n)) {
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
    printf("benchmark sink=%lu\n", (unsigned long)sink);
    return 0;
}
