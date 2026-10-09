/* Fair PS2 EE CRC32+copy competition: braid C+memcpy, MMI Chorba+memcpy,
 * and fused MMI Chorba. A fused-vs-MMI-only result is not sufficient to
 * establish an improvement over the production scalar checksum path.
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
    if (begin == (clock_t)-1 || end == (clock_t)-1 || end < begin)
        return (clock_t)-1;
    return end - begin;
}

int main(void) {
    static const char *const names[] = {"generic+copy", "mmi+copy", "fused"};
    static const copy_fn variants[] = {
        crc32_copy_braid, crc32_copy_chorba_mmi_twopass,
        crc32_copy_chorba_mmi_fused
    };
    ps2_bench_candidates("chorba_copy", names, 3, 1);
    static const size_t lengths[] = {1024, 4096, 8192, 32768, MAX_BYTES};
    static const unsigned iters[] = {600, 300, 200, 60, 8};
    static const unsigned offsets[] = {0, 1, 7, 15};
    uint32_t rng = 0x9e3779b9u;
    for (unsigned i = 0; i < sizeof(source); ++i) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        source[i] = (uint8_t)rng;
    }
    printf("MMI Chorba+copy three-way, CLOCKS_PER_SEC=%lu\n",
           (unsigned long)CLOCKS_PER_SEC);
    puts("length src_offset dst_offset iterations braid_ticks mmi_twopass_ticks fused_ticks braid_over_mmi braid_over_fused");
    for (unsigned li = 0; li < sizeof(lengths)/sizeof(lengths[0]); ++li)
        for (unsigned so = 0; so < sizeof(offsets)/sizeof(offsets[0]); ++so)
            for (unsigned di = 0; di < sizeof(offsets)/sizeof(offsets[0]); ++di) {
                ps2_bench_case("CRC copy cases", li * 16 + so * 4 + di + 1, 80);
                size_t n = lengths[li];
                const uint8_t *src = source + offsets[so];
                uint8_t *dst = dest + offsets[di];
                const uint32_t expected = crc32_braid(1u, src, n);
                int bad = 0;
                for (unsigned v = 0; v < 3; ++v) {
                    memset(dest, 0xa5, sizeof(dest));
                    uint32_t actual = variants[v](1u, dst, src, n);
                    int ok = actual == expected && memcmp(dst, src, n) == 0 &&
                             dst[n] == 0xa5u;
                    ps2_bench_check(v, ok);
                    if (!ok) {
                        printf("CRC+copy FAIL variant=%u n=%lu so=%u di=%u\n",
                               v, (unsigned long)n, offsets[so], offsets[di]);
                        bad = 1;
                    }
                }
                if (bad) return 1;
                clock_t ticks[3];
                unsigned reps = ps2_bench_iterations(iters[li]);
                if ((li + so + di) & 1u) {
                    for (int v = 2; v >= 0; --v)
                        ticks[v] = run(variants[v], dst, src, n, reps);
                } else {
                    for (unsigned v = 0; v < 3; ++v)
                        ticks[v] = run(variants[v], dst, src, n, reps);
                }
                for (unsigned v = 0; v < 3; ++v) ps2_bench_ticks(v, ticks[v]);
                if (ticks[0] <= 0 || ticks[1] <= 0 || ticks[2] <= 0)
                    printf("%lu %u %u %u clock_unavailable_or_too_coarse\n",
                           (unsigned long)n, offsets[so], offsets[di], reps);
                else
                    printf("%lu %u %u %u %ld %ld %ld %.3f %.3f\n",
                           (unsigned long)n, offsets[so], offsets[di], reps,
                           (long)ticks[0], (long)ticks[1], (long)ticks[2],
                           (double)ticks[0]/(double)ticks[1],
                           (double)ticks[0]/(double)ticks[2]);
            }
    printf("benchmark sink=%lu\n", (unsigned long)sink);
    return 0;
}
