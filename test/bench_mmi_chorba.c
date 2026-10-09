/* Compare EE Chorba against generic braid and generic Chorba CRC32.
 * Run on PS2 after building with WITH_MMI_CHORBA=ON.
 * clock() can be too coarse on EE; prefer a hardware cycle counter if so. */
#include "zbuild.h"
#include "arch_functions.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "ps2/bench_display.h"

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
    static const char *const names[] = {
        "braid", "single", "paired", "threshold1K", "threshold4K", "threshold8K"
    };
    ps2_bench_candidates("chorba", names, CHORBA_VARIANTS, 1);
    static const size_t sizes[] = {
        128, 1023, 1024, 1025, 2048, 4095, 4096, 4097,
        8191, 8192, 8193, 32768, 262144, MAX_BENCH
    };
    static const unsigned iterations[] = {
        8000, 4000, 4000, 4000, 2500, 2000, 2000, 2000,
        1000, 1000, 1000, 300, 40, 10
    };
    static const crc_func variants[] = {
        crc32_braid, crc32_chorba_mmi_single, crc32_chorba_mmi_paired,
        crc32_chorba_mmi_threshold1024,
        crc32_chorba_mmi_threshold4096,
        crc32_chorba_mmi_threshold8192
    };
    uint32_t rng = 0x5a17e4b3u;
    for (unsigned i = 0; i < sizeof(data); ++i) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        data[i] = (uint8_t)rng;
    }
    printf("MMI Chorba A/B CLOCKS_PER_SEC=%lu\n",
           (unsigned long)CLOCKS_PER_SEC);
    puts("CRC32 bench: columns size offset reps, one ticks/variant, then braid/variant ratios");
    puts("variant order: braid single paired threshold1K threshold4K threshold8K"
         );

    for (unsigned i = 0; i < sizeof(sizes)/sizeof(sizes[0]); ++i)
        for (unsigned align = 0; align <= 1; ++align) {
            ps2_bench_case("CRC cases", i * 2 + align + 1, 28);
                const uint8_t *p = data + align;
            const uint32_t expected = crc32_braid(0u, p, sizes[i]);
            int failed = 0;
            for (unsigned v = 1; v < sizeof(variants)/sizeof(variants[0]); ++v) {
                uint32_t got = variants[v](0u, p, sizes[i]);
                ps2_bench_check(v, got == expected);
                if (got != expected) {
                    printf("CRC mismatch size=%lu align=%u variant=%u expected=%08lx got=%08lx\n",
                           (unsigned long)sizes[i], align, v,
                           (unsigned long)expected, (unsigned long)got);
                    failed = 1;
                }
            }
            ps2_bench_check(0, 1);
            if (failed) return 1;
            clock_t ticks[CHORBA_VARIANTS];
            unsigned repetitions = ps2_bench_iterations(iterations[i]);
            if ((i + align) & 1u) {
                for (int v = CHORBA_VARIANTS - 1; v >= 0; --v)
                    ticks[v] = run(variants[v], p, sizes[i], repetitions);
            } else {
                for (unsigned v = 0; v < CHORBA_VARIANTS; ++v)
                    ticks[v] = run(variants[v], p, sizes[i], repetitions);
            }
            for (unsigned v = 0; v < CHORBA_VARIANTS; ++v) ps2_bench_ticks(v, ticks[v]);
            int valid = 1;
            for (unsigned v = 0; v < CHORBA_VARIANTS; ++v)
                if (ticks[v] <= 0) valid = 0;
            printf("%lu %u %u", (unsigned long)sizes[i], align, repetitions);
            if (!valid) puts(" clock_unavailable_or_too_coarse");
            else {
                for (unsigned v = 0; v < CHORBA_VARIANTS; ++v)
                    printf(" %ld", (long)ticks[v]);
                for (unsigned v = 1; v < CHORBA_VARIANTS; ++v)
                    printf(" %.3f", (double)ticks[0] / (double)ticks[v]);
                putchar('\n');
            }
        }
    printf("keep_result=%lu\n", (unsigned long)keep_result);
    return 0;
}
