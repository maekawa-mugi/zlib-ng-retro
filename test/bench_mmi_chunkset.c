/* PS2 EE LZ77 history copy schedules. Runs on real EE hardware.
 * Always verifies both routines against a sequential history-copy model.
 */
#include "zbuild.h"
#include "arch_functions.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
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
    return end == buffer + start + length &&
           memcmp(buffer, checkbuf, sizeof(buffer)) == 0;
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

int main(void) {
    static const unsigned distances[] = {
        1, 2, 3, 4, 7, 8, 15, 16, 32, 48, 64, 80, 128, 256, 512
    };
    static const unsigned lengths[] = {32, 64, 65, 128, 256, 1024};
    static const unsigned offsets[] = {0, 1, 7, 15};
    static const copy_fn variants[] = {
        chunkmemset_safe_c, chunkmemset_safe_mmi_serial,
        chunkmemset_safe_mmi_burst, chunkmemset_safe_mmi_pattern
    };
    const unsigned variant_count = (unsigned)(sizeof(variants)/sizeof(variants[0]));
    printf("MMI LZ77 copy A/B CLOCKS_PER_SEC=%lu\n",
           (unsigned long)CLOCKS_PER_SEC);
    puts("distance length offset C_ticks serial_ticks burst_ticks pattern_ticks C_over_serial C_over_burst C_over_pattern");
    for (unsigned d = 0; d < sizeof(distances)/sizeof(distances[0]); ++d)
        for (unsigned l = 0; l < sizeof(lengths)/sizeof(lengths[0]); ++l)
            for (unsigned o = 0; o < sizeof(offsets)/sizeof(offsets[0]); ++o) {
                unsigned dist = distances[d], len = lengths[l], off = offsets[o];
                clock_t times[4];
                for (unsigned v = 0; v < variant_count; ++v) {
                    if (!validate(variants[v], dist, len, off)) {
                        printf("MMI copy benchmark FAIL variant=%u dist=%u len=%u off=%u\n",
                               v, dist, len, off);
                        return 1;
                    }
                }
                const unsigned iterations = len >= 1024 ? 2500u : 8000u;
                if ((d + l + o) & 1u) {
                    for (int v = (int)variant_count - 1; v >= 0; --v)
                        times[v] = bench(variants[v], dist, len, off, iterations);
                } else {
                    for (unsigned v = 0; v < variant_count; ++v)
                        times[v] = bench(variants[v], dist, len, off, iterations);
                }
                if (times[0] <= 0 || times[1] <= 0 ||
                    times[2] <= 0 || times[3] <= 0) {
                    printf("%u %u %u clock_unavailable_or_too_coarse\n",
                           dist, len, off);
                } else {
                    printf("%u %u %u %ld %ld %ld %ld %.3f %.3f %.3f\n",
                           dist, len, off, (long)times[0], (long)times[1],
                           (long)times[2], (long)times[3],
                           (double)times[0] / (double)times[1],
                           (double)times[0] / (double)times[2],
                           (double)times[0] / (double)times[3]);
                }
            }
    printf("benchmark sink=%lu\n", (unsigned long)sink);
    return 0;
}
