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
#include <string.h>
#include <time.h>

#ifndef MIPS_MMI
#  error "Build with WITH_MMI=ON"
#endif
#define MAX_BENCH (256u * 1024u)
static uint8_t input[MAX_BENCH];
static uint8_t packed[MAX_BENCH + 1024u];
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
                int level = levels[leveli];
                unsigned count = iterations[li];
                z_uintmax_t used = sizeof(packed);
                z_uintmax_t decoded = sizeof(unpacked);

                int status = PREFIX(compress2)(packed, &used, input,
                                               (z_uintmax_t)len, level);
                if (status != Z_OK) {
                    printf("MMI roundtrip bench compress FAIL p=%u level=%d len=%lu status=%d\n",
                           pattern, level, (unsigned long)len, status);
                    return 1;
                }
                status = PREFIX(uncompress)(unpacked, &decoded, packed, used);
                if (status != Z_OK || (size_t)decoded != len ||
                    memcmp(input, unpacked, len) != 0) {
                    printf("MMI roundtrip bench decode FAIL p=%u level=%d len=%lu status=%d\n",
                           pattern, level, (unsigned long)len, status);
                    return 1;
                }

                clock_t compress_start = clock();
                for (unsigned i = 0; i < count; ++i) {
                    z_uintmax_t size = sizeof(packed);
                    status = PREFIX(compress2)(packed, &size, input,
                                               (z_uintmax_t)len, level);
                    if (status != Z_OK || size != used) {
                        printf("MMI roundtrip bench repeated compress FAIL p=%u len=%lu level=%d\n",
                               pattern, (unsigned long)len, level);
                        return 1;
                    }
                    sink ^= (uint32_t)size;
                }
                clock_t compress_stop = clock();

                clock_t decompress_start = clock();
                for (unsigned i = 0; i < count; ++i) {
                    z_uintmax_t size = sizeof(unpacked);
                    status = PREFIX(uncompress)(unpacked, &size, packed, used);
                    if (status != Z_OK || (size_t)size != len) {
                        printf("MMI roundtrip bench repeated decode FAIL p=%u len=%lu level=%d\n",
                               pattern, (unsigned long)len, level);
                        return 1;
                    }
                    sink ^= unpacked[i % len];
                }
                clock_t decompress_stop = clock();
                if (memcmp(input, unpacked, len) != 0) {
                    puts("MMI roundtrip benchmark output mismatch");
                    return 1;
                }

                clock_t c = compress_stop - compress_start;
                clock_t u = decompress_stop - decompress_start;
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
    return 0;
}
