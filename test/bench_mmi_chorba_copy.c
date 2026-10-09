/* Benchmark EE CRC32 Chorba plus memcpy against a one-pass copy schedule.
 * The same data is checked against CRC braid before each timing run.
 * clock() is approximate on PS2 Linux; prefer EE performance counters.
 */
#include "zbuild.h"
#include "arch_functions.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "ps2/bench_display.h"

#ifndef MIPS_MMI_CHORBA
#  error "Requires WITH_MMI_CHORBA=ON"
#endif

#define MAX_BYTES (256u * 1024u)
static uint8_t source[MAX_BYTES + 32] ALIGNED_(16);
static uint8_t dest[MAX_BYTES + 32] ALIGNED_(16);
static volatile uint32_t sink;
typedef uint32_t (*copy_fn)(uint32_t, uint8_t *, const uint8_t *, size_t);

static clock_t run(copy_fn fn, uint8_t *dst, const uint8_t *src,
                   size_t n, unsigned iterations) {
    clock_t begin = clock();
    uint32_t value = 0;
    for (unsigned i = 0; i < iterations; ++i)
        value ^= fn(i, dst, src, n) + i;
    clock_t end = clock();
    sink ^= value ^ dst[0];
    if (begin == (clock_t)-1 || end == (clock_t)-1)
        return (clock_t)-1;
    return end - begin;
}

int main(void) {
    static const char *const names[] = {"two-pass", "fused"};
    ps2_bench_candidates("chorba_copy", names, 2, 1);
    static const size_t lengths[] = {1024, 4096, 8192, 32768, MAX_BYTES};
    static const unsigned iters[] = {600, 300, 200, 60, 8};
    static const unsigned offsets[] = {0, 1, 7, 15};
    uint32_t rng = 0x9e3779b9u;
    for (unsigned i = 0; i < sizeof(source); ++i) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        source[i] = (uint8_t)rng;
    }

    printf("MMI Chorba copy A/B CLOCKS_PER_SEC=%lu\n",
           (unsigned long)CLOCKS_PER_SEC);
    puts("length src_offset dst_offset iterations twopass_ticks fused_ticks twopass_over_fused");
    for (unsigned li = 0; li < sizeof(lengths)/sizeof(lengths[0]); ++li)
        for (unsigned so = 0; so < sizeof(offsets)/sizeof(offsets[0]); ++so)
            for (unsigned di = 0; di < sizeof(offsets)/sizeof(offsets[0]); ++di) {
                ps2_bench_case("CRC copy cases", li * 16 + so * 4 + di + 1, 80);
                size_t n = lengths[li];
                const uint8_t *src = source + offsets[so];
                uint8_t *dst = dest + offsets[di];
                const uint32_t expected = crc32_braid(1u, src, n);
                memset(dest, 0xa5, sizeof(dest));
                uint32_t two = crc32_copy_chorba_mmi_twopass(1u, dst, src, n);
                int ok_a = two == expected && memcmp(dst, src, n) == 0 && dst[n] == 0xa5u;
                ps2_bench_check(0, ok_a);
                if (!ok_a) {
                    printf("Chorba two-pass FAIL len=%lu so=%u di=%u\n",
                           (unsigned long)n, offsets[so], offsets[di]);
                    /* Also check B below. */
                }
                memset(dest, 0xa5, sizeof(dest));
                uint32_t fused = crc32_copy_chorba_mmi_fused(1u, dst, src, n);
                int ok_b = fused == expected && memcmp(dst, src, n) == 0 && dst[n] == 0xa5u;
                ps2_bench_check(1, ok_b);
                if (!ok_b) {
                    printf("Chorba fused FAIL len=%lu so=%u di=%u\n",
                           (unsigned long)n, offsets[so], offsets[di]);
                    return 1;
                }

                if (!ok_a || !ok_b) return 1;
                clock_t a, b;
                unsigned repetitions = ps2_bench_iterations(iters[li]);
                if ((li + so + di) & 1u) {
                    b = run(crc32_copy_chorba_mmi_fused, dst, src, n, repetitions);
                    a = run(crc32_copy_chorba_mmi_twopass, dst, src, n, repetitions);
                } else {
                    a = run(crc32_copy_chorba_mmi_twopass, dst, src, n, repetitions);
                    b = run(crc32_copy_chorba_mmi_fused, dst, src, n, repetitions);
                }
                ps2_bench_ticks(0, a);
                ps2_bench_ticks(1, b);
                if (a <= 0 || b <= 0) {
                    printf("%lu %u %u %u clock_unavailable_or_too_coarse\n",
                           (unsigned long)n, offsets[so], offsets[di], repetitions);
                } else {
                    printf("%lu %u %u %u %ld %ld %.3f\n",
                           (unsigned long)n, offsets[so], offsets[di], repetitions,
                           (long)a, (long)b, (double)a / (double)b);
                }
            }
    printf("benchmark sink=%lu\n", (unsigned long)sink);
    return 0;
}
