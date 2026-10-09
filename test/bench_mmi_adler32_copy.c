/* EE Adler32 + copy: two separate traversals vs fused MMI traversal.
 * Execute the output on PS2; clock() may have insufficient resolution.
 */
#include "zbuild.h"
#include "arch_functions.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "ps2/bench_display.h"
#ifndef MIPS_MMI_ADLER32
#  error "Requires WITH_MMI_ADLER32=ON"
#endif

static uint8_t source[65536 + 32] ALIGNED_(16);
static uint8_t dest[65536 + 32] ALIGNED_(16);
static volatile uint32_t sink;
typedef uint32_t (*copy_fn)(uint32_t, uint8_t *, const uint8_t *, size_t);

static clock_t run(copy_fn fn, uint8_t *dst, const uint8_t *src,
                   size_t length, unsigned iterations) {
    uint32_t acc = 0;
    clock_t begin = clock();
    for (unsigned i = 0; i < iterations; ++i)
        acc ^= fn(i + 1u, dst, src, length) + i;
    clock_t end = clock();
    sink ^= acc ^ dst[0];
    if (begin == (clock_t)-1 || end == (clock_t)-1)
        return (clock_t)-1;
    return end - begin;
}

int main(void) {
    static const char *const names[] = {"two-pass", "fused"};
    ps2_bench_candidates("adler32_copy", names, 2, 1);
    static const size_t lengths[] = {16, 64, 1024, 8192, 65536};
    static const unsigned iterations[] = {200000, 100000, 10000, 1200, 150};
    static const unsigned offsets[] = {0, 1, 7, 15};
    uint32_t rng = 0x9e3779b9u;
    for (unsigned i = 0; i < sizeof(source); ++i) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        source[i] = (uint8_t)rng;
    }
    printf("MMI Adler copy A/B CLOCKS_PER_SEC=%lu\n",
           (unsigned long)CLOCKS_PER_SEC);
    puts("size src_offset dst_offset iterations twopass_ticks fused_ticks twopass_over_fused");
    for (unsigned li = 0; li < sizeof(lengths)/sizeof(lengths[0]); ++li) {
        size_t n = lengths[li];
        for (unsigned so = 0; so < sizeof(offsets)/sizeof(offsets[0]); ++so)
            for (unsigned di = 0; di < sizeof(offsets)/sizeof(offsets[0]); ++di) {
                ps2_bench_case("Adler copy cases", li * 16 + so * 4 + di + 1, 80);
                const uint8_t *src = source + offsets[so];
                uint8_t *dst = dest + offsets[di];
                uint32_t expected = adler32_c(1u, src, n);
                memset(dest, 0xa5, sizeof(dest));
                uint32_t twopass = adler32_copy_mmi_twopass(1u, dst, src, n);
                int ok_a = twopass == expected && memcmp(src, dst, n) == 0 && dst[n] == 0xa5u;
                ps2_bench_check(0, ok_a);
                if (!ok_a) {
                    printf("MMI Adler copy two-pass FAIL n=%lu so=%u di=%u\n",
                           (unsigned long)n, offsets[so], offsets[di]);
                    /* Also check B below. */
                }
                memset(dest, 0xa5, sizeof(dest));
                uint32_t fused = adler32_copy_mmi_fused(1u, dst, src, n);
                int ok_b = fused == expected && memcmp(src, dst, n) == 0 && dst[n] == 0xa5u;
                ps2_bench_check(1, ok_b);
                if (!ok_b) {
                    printf("MMI Adler copy fused FAIL n=%lu so=%u di=%u\n",
                           (unsigned long)n, offsets[so], offsets[di]);
                    return 1;
                }
                if (!ok_a || !ok_b) return 1;
                clock_t first, second;
                unsigned iters = ps2_bench_iterations(iterations[li]);
                if ((li + so + di) & 1u) {
                    second = run(adler32_copy_mmi_fused, dst, src, n, iters);
                    first = run(adler32_copy_mmi_twopass, dst, src, n, iters);
                } else {
                    first = run(adler32_copy_mmi_twopass, dst, src, n, iters);
                    second = run(adler32_copy_mmi_fused, dst, src, n, iters);
                }
                ps2_bench_ticks(0, first);
                ps2_bench_ticks(1, second);
                if (first <= 0 || second <= 0)
                    printf("%lu %u %u %u clock_unavailable_or_too_coarse\n",
                           (unsigned long)n, offsets[so], offsets[di], iters);
                else
                    printf("%lu %u %u %u %ld %ld %.3f\n",
                           (unsigned long)n, offsets[so], offsets[di], iters,
                           (long)first, (long)second,
                           (double)first / (double)second);
            }
    }
    printf("benchmark sink=%lu\n", (unsigned long)sink);
    return 0;
}
