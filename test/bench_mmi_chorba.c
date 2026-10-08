/* Compare EE Chorba against generic braid and generic Chorba CRC32.
 * Run on PS2 after building with WITH_MMI_CHORBA=ON.
 * clock() can be too coarse on EE; prefer a hardware cycle counter if so. */
#include "zbuild.h"
#include "arch_functions.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifndef MIPS_MMI_CHORBA
#  error "Build with WITH_MMI_CHORBA=ON"
#endif

#define MAX_BENCH (1024u * 1024u)
static uint8_t data[MAX_BENCH + 16] ALIGNED_(16);
static volatile uint32_t keep_result;
typedef uint32_t (*crc_func)(uint32_t, const uint8_t *, size_t);

static clock_t run(crc_func fn, const uint8_t *buf, size_t size,
                   unsigned iterations) {
    uint32_t acc = 0;
    clock_t start = clock();
    for (unsigned i = 0; i < iterations; i++)
        acc ^= fn(i, buf, size) + i;
    clock_t stop = clock();
    keep_result ^= acc;
    if (start == (clock_t)-1 || stop == (clock_t)-1)
        return (clock_t)-1;
    return stop - start;
}

int main(void) {
    static const size_t sizes[] = {128, 1024, 4096, 8192, 32768,
                                   262144, MAX_BENCH};
    static const unsigned iterations[] = {8000, 4000, 2000, 1000,
                                          300, 40, 10};
    uint32_t rng = 0x5a17e4b3u;
    for (unsigned i = 0; i < sizeof(data); i++) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        data[i] = (uint8_t)rng;
    }

    printf("MMI Chorba A/B, CLOCKS_PER_SEC=%lu\n",
           (unsigned long)CLOCKS_PER_SEC);
    puts("size offset count braid_ticks mmi_ticks C_over_MMI");

    for (unsigned i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
        for (unsigned align = 0; align <= 1; align++) {
            const uint8_t *p = data + align;
            uint32_t expect = crc32_braid(0, p, sizes[i]);
            uint32_t got = crc32_chorba_mmi(0, p, sizes[i]);
            if (expect != got) {
                printf("CRC mismatch size=%lu align=%u braid=%08lx mmi=%08lx\n",
                       (unsigned long)sizes[i], align,
                       (unsigned long)expect, (unsigned long)got);
                return 1;
            }
            clock_t b, m;
            if ((i + align) & 1u) {
                m = run(crc32_chorba_mmi, p, sizes[i], iterations[i]);
                b = run(crc32_braid, p, sizes[i], iterations[i]);
            } else {
                b = run(crc32_braid, p, sizes[i], iterations[i]);
                m = run(crc32_chorba_mmi, p, sizes[i], iterations[i]);
            }
            if (b <= 0 || m <= 0) {
                printf("%lu %u %u clock_unavailable_or_too_coarse\n",
                       (unsigned long)sizes[i], align, iterations[i]);
            } else {
                printf("%lu %u %u %ld %ld %.3f\n",
                       (unsigned long)sizes[i], align, iterations[i],
                       (long)b, (long)m, (double)b/(double)m);
            }
        }
    }
    printf("keep_result=%lu\n", (unsigned long)keep_result);
    return 0;
}
