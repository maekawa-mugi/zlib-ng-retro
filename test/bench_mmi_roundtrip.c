/* End-to-end PS2 EE deflate/inflate timings and full data verification.
 * Cross-build the same source and flags with individual WITH_MMI_* A/B
 * switches. Timing uses clock(); EE performance counters are preferable.
 */
#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ps2/bench_display.h"
#include "ps2/stress_diagnostic.h"

#ifndef MIPS_MMI
#  error "Build with WITH_MMI=ON"
#endif
#define MAX_BENCH (256u * 1024u)
static uint8_t input[MAX_BENCH];
static uint8_t unpacked[MAX_BENCH];
static volatile uint32_t sink;

static void generate(unsigned pattern, size_t len) {
    uint32_t seed = 0x12345678u;
    for (size_t i = 0; i < len; ++i) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        switch (pattern) {
        case 0: input[i] = (uint8_t)seed; break;
        case 1: input[i] = (uint8_t)(i & 15u); break;
        case 2: input[i] = 0; break;
        default:
            input[i] = ((i / 4096u) & 1u) ?
                (uint8_t)(i % 31u) : (uint8_t)(seed & 255u);
            break;
        }
    }
}

int main(void) {
    static const char *const names[] = {"compress", "decode"};
    ps2_bench_candidates("roundtrip", names, 2, 0);
    /* Level 1's quick strategy can expand random data by more than 1KB.
     * Allocate once, outside the timed loops, using the library's bound. */
    z_uintmax_t capacity = PREFIX(compressBound)(MAX_BENCH);
    uint8_t *packed = malloc((size_t)capacity);
    if (!packed) {
        ps2_bench_check(0, 0);
        mmi_stress_fail("roundtrip allocation FAIL capacity=%lu\n", (unsigned long)capacity);
        return 1;
    }
    static const size_t lengths[] = {4096, 65536, MAX_BENCH};
    static const unsigned iterations[] = {250, 40, 12};
    static const int levels[] = {1, 6, 9};
    printf("PS2 MMI roundtrip benchmark CLOCKS_PER_SEC=%lu\n",
           (unsigned long)CLOCKS_PER_SEC);
    puts("pattern level input_size compressed_size iterations compress_ticks decompress_ticks");

    for (unsigned pattern = 0; pattern < 4; ++pattern)
        for (unsigned li = 0; li < sizeof(lengths)/sizeof(lengths[0]); ++li) {
            size_t len = lengths[li];
            generate(pattern, len);
            for (unsigned leveli = 0; leveli < sizeof(levels)/sizeof(levels[0]); ++leveli) {
                ps2_bench_case("roundtrip cases", (pattern * 3 + li) * 3 + leveli + 1, 36);
                int level = levels[leveli];
                unsigned count = ps2_bench_iterations(iterations[li]);
                z_uintmax_t used = capacity;
                z_uintmax_t decoded = sizeof(unpacked);

                int status = PREFIX(compress2)(packed, &used, input,
                                               (z_uintmax_t)len, level);
                ps2_bench_check(0, status == Z_OK);
                if (status != Z_OK) {
                    mmi_stress_fail("MMI roundtrip bench compress FAIL p=%u level=%d len=%lu status=%d\n",
                           pattern, level, (unsigned long)len, status);
                    free(packed);
                    return 1;
                }
                status = PREFIX(uncompress)(unpacked, &decoded, packed, used);
                int decoded_ok = status == Z_OK && (size_t)decoded == len && memcmp(input, unpacked, len) == 0;
                ps2_bench_check(1, decoded_ok);
                if (!decoded_ok) {
                    mmi_stress_fail("MMI roundtrip bench decode FAIL p=%u level=%d len=%lu status=%d\n",
                           pattern, level, (unsigned long)len, status);
                    free(packed);
                    return 1;
                }

                clock_t compress_start = clock();
                for (unsigned i = 0; i < count; ++i) {
                    z_uintmax_t size = capacity;
                    status = PREFIX(compress2)(packed, &size, input,
                                               (z_uintmax_t)len, level);
                    if (status != Z_OK || size != used) {
                        ps2_bench_check(0, 0);
                        mmi_stress_fail("MMI roundtrip bench repeated compress FAIL p=%u len=%lu level=%d\n",
                               pattern, (unsigned long)len, level);
                        free(packed);
                        return 1;
                    }
                    sink ^= (uint32_t)size;
                }
                clock_t compress_stop = clock();
                ps2_bench_check(0, 1);  /* Never call screen hooks in the timed loop. */

                clock_t decompress_start = clock();
                for (unsigned i = 0; i < count; ++i) {
                    z_uintmax_t size = sizeof(unpacked);
                    status = PREFIX(uncompress)(unpacked, &size, packed, used);
                    if (status != Z_OK || (size_t)size != len) {
                        ps2_bench_check(1, 0);
                        mmi_stress_fail("MMI roundtrip bench repeated decode FAIL p=%u len=%lu level=%d\n",
                               pattern, (unsigned long)len, level);
                        free(packed);
                        return 1;
                    }
                    sink ^= unpacked[i % len];
                }
                clock_t decompress_stop = clock();
                ps2_bench_check(1, 1);  /* Timed batch decoded successfully. */
                ps2_bench_check(1, memcmp(input, unpacked, len) == 0);
                if (memcmp(input, unpacked, len) != 0) {
                    mmi_stress_fail("MMI roundtrip benchmark output mismatch\n");
                    free(packed);
                    return 1;
                }

                clock_t c = compress_stop - compress_start;
                clock_t u = decompress_stop - decompress_start;
                ps2_bench_ticks(0, compress_start == (clock_t)-1 || compress_stop == (clock_t)-1 ? (clock_t)-1 : c);
                ps2_bench_ticks(1, decompress_start == (clock_t)-1 || decompress_stop == (clock_t)-1 ? (clock_t)-1 : u);
                if (compress_start == (clock_t)-1 || compress_stop == (clock_t)-1 ||
                    decompress_start == (clock_t)-1 || decompress_stop == (clock_t)-1 ||
                    c <= 0 || u <= 0) {
                    printf("%u %d %lu %lu %u clock_unavailable_or_too_coarse\n",
                           pattern, level, (unsigned long)len,
                           (unsigned long)used, count);
                } else {
                    printf("%u %d %lu %lu %u %ld %ld\n", pattern, level,
                           (unsigned long)len, (unsigned long)used, count,
                           (long)c, (long)u);
                }
            }
        }
    printf("benchmark sink=%lu\n", (unsigned long)sink);
    free(packed);
    return 0;
}
