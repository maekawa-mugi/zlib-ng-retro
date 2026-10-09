/* PS2 EE LZ77 history copy schedules. Runs on real EE hardware.
 * Always verifies both routines against a sequential history-copy model.
 */
#include "zbuild.h"
#include "arch_functions.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "ps2/bench_display.h"
#ifndef MIPS_MMI
#  error "Requires WITH_MMI=ON"
#endif

static uint8_t buffer[8192] ALIGNED_(16);
static uint8_t checkbuf[8192] ALIGNED_(16);
static volatile uint32_t sink;
typedef uint8_t *(*copy_fn)(uint8_t *, uint8_t *, size_t, size_t);

static void init(uint8_t *p) {
    for (unsigned i = 0; i < sizeof(buffer); ++i)
        p[i] = (uint8_t)((i * 43u + (i >> 3) * 17u) & 255u);
}
static int validate(copy_fn fn, unsigned distance, unsigned length,
                    unsigned offset) {
    const unsigned start = 2048 + offset;
    init(buffer);
    memcpy(checkbuf, buffer, sizeof(buffer));
    for (unsigned i = 0; i < length; ++i)
        checkbuf[start + i] = checkbuf[start + i - distance];
    uint8_t *end = fn(buffer + start, buffer + start - distance,
                      length, sizeof(buffer) - start);
    /* The API allows writes beyond requested length, but only inside
     * 'left' output capacity. Check the exact requested output and the
     * untouched prefix, without rejecting legal whole-chunk stores. */
    return end == buffer + start + length &&
           memcmp(buffer, checkbuf, start + length) == 0;
}

static clock_t bench(copy_fn fn, unsigned dist, unsigned len,
                     unsigned offset, unsigned iters) {
    init(buffer);
    uint8_t *out = buffer + 2048 + offset;
    clock_t begin = clock();
    for (unsigned i = 0; i < iters; ++i) {
        uint8_t *end = fn(out, out - dist, len, sizeof(buffer) - 2048 - offset);
        sink ^= end[0];
    }
    clock_t end = clock();
    if (begin == (clock_t)-1 || end == (clock_t)-1)
        return (clock_t)-1;
    return end - begin;
}

/* Different distances exercise completely different microkernels. Never
 * pool them in a single A/B ranking (the original 70% regression did so). */
int main(void) {
    static const unsigned shortdist[] = {1, 2, 4, 8};
    static const unsigned longdist[] = {64, 80, 128, 256, 512};
    static const unsigned lengths[] = {64, 65, 128, 256, 1024};
    static const unsigned offsets[] = {0, 1, 7, 15};
    static const copy_fn short_fns[] = {
        chunkmemset_safe_c, chunkmemset_safe_mmi_serial,
        chunkmemset_safe_mmi_pattern, chunkmemset_safe_mmi_pattern128
    };
    static const copy_fn long_fns[] = {
        chunkmemset_safe_c, chunkmemset_safe_mmi_serial,
        chunkmemset_safe_mmi_burst
    };
    static const char *const short_names[] = {
        "generic", "serial", "pattern64", "pattern128"
    };
    static const char *const long_names[] = {
        "generic", "serial", "load4"
    };
    for (unsigned group = 0; group < 2; ++group) {
        const copy_fn *functions = group ? long_fns : short_fns;
        const unsigned count = group ? 3 : 4;
        const unsigned *distances = group ? longdist : shortdist;
        const unsigned nd = group ? 5 : 4;
        ps2_bench_candidates(group ? "lz77_long" : "lz77_short",
                             group ? long_names : short_names, count, 1);
        for (unsigned d = 0; d < nd; ++d)
            for (unsigned l = 0; l < sizeof(lengths)/sizeof(lengths[0]); ++l)
                for (unsigned o = 0; o < sizeof(offsets)/sizeof(offsets[0]); ++o) {
                    unsigned dist = distances[d], len = lengths[l], off = offsets[o];
                    ps2_bench_case(group ? "long LZ77" : "short LZ77",
                                   (d * 5 + l) * 4 + o + 1, nd * 5 * 4);
                    clock_t ticks[4];
                    /* The exact reference and every contender are verified
                     * once before timing, not once per iteration. */
                    for (unsigned v = 0; v < count; ++v) {
                        int ok = validate(functions[v], dist, len, off);
                        ps2_bench_check(v, ok);
                        if (!ok) {
                            printf("LZ77 FAIL group=%u v=%u dist=%u len=%u off=%u\n",
                                   group, v, dist, len, off);
                            return 1;
                        }
                    }
                    unsigned n = ps2_bench_iterations(len == 1024 ? 2500u : 8000u);
                    if ((d + l + o) & 1u) {
                        for (int v = (int)count - 1; v >= 0; --v)
                            ticks[v] = bench(functions[v], dist, len, off, n);
                    } else {
                        for (unsigned v = 0; v < count; ++v)
                            ticks[v] = bench(functions[v], dist, len, off, n);
                    }
                    /* The rank reporter prints a separate CSV line.
                     * Never call it while an LZ77_CASE row is unfinished. */
                    printf("LZ77_CASE,%s,%u,%u,%u", group ? "long" : "short",
                           dist, len, off);
                    for (unsigned v = 0; v < count; ++v)
                        printf(",%ld", (long)ticks[v]);
                    putchar('\n');
                    for (unsigned v = 0; v < count; ++v)
                        ps2_bench_ticks(v, ticks[v]);
                }
    }
    printf("benchmark sink=%lu\n", (unsigned long)sink);
    return 0;
}
