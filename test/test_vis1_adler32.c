/* Verify VIS1 Adler-32 and its copying variant against a scalar oracle.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif
#include "zbuild.h"
#include "arch/sparc/sparc_features.h"

#if defined(HAVE_SPARC_GETAUXVAL)
#  include <sys/auxv.h>
#  include <elf.h>
#endif

uint32_t adler32_vis1(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t adler32_copy_vis1(uint32_t adler, uint8_t *dst, const uint8_t *buf, size_t len);

#define ADLER_BASE 65521u

static uint32_t scalar_reference(uint32_t adler, const uint8_t *p, size_t n) {
    uint64_t a = adler & 0xffffu;
    uint64_t b = adler >> 16;
    for (size_t i = 0; i < n; ++i) {
        a += p[i];
        b += a;
    }
    return (uint32_t)(a % ADLER_BASE) | ((uint32_t)(b % ADLER_BASE) << 16);
}

int main(void) {
    static const size_t lens[] = {
        0, 1, 2, 3, 4, 7, 15, 16, 31, 63, 64, 65,
        127, 128, 255, 256, 5535, 5536, 5551, 5552,
        5553, 8192, 16384, 65536
    };
    static const uint32_t seeds[] = {1, 0, 0x10203040u, 0xfff0fff0u};
    uint8_t buf[65536 + 8] ALIGNED_(8);
    uint8_t copy[65536 + 8 + 2] ALIGNED_(8);
    unsigned cases = 0;

#if defined(HAVE_SPARC_GETAUXVAL) && !defined(DISABLE_RUNTIME_CPU_DETECTION)
    if (!sparc_hwcap_has_vis1(getauxval(AT_HWCAP))) {
        puts("VIS1 not available on this CPU: skipping direct VIS1 Adler-32 test");
        return 77; /* CTest SKIP_RETURN_CODE */
    }
#endif

    for (unsigned shift = 0; shift < 8; ++shift) {
        uint8_t *p = buf + shift;
        for (unsigned pattern = 0; pattern < 3; ++pattern) {
            for (size_t i = 0; i < 65536; ++i) {
                p[i] = pattern == 0 ? 0xffu : pattern == 1 ? (uint8_t)i :
                       (uint8_t)((i * 41u + (i >> 3) * 23u) & 0xffu);
            }
            for (size_t n = 0; n < sizeof(lens) / sizeof(lens[0]); ++n) {
                for (size_t s = 0; s < sizeof(seeds) / sizeof(seeds[0]); ++s) {
                    const size_t len = lens[n];
                    uint32_t expected = scalar_reference(seeds[s], p, len);
                    uint32_t actual = adler32_vis1(seeds[s], p, len);
                    if (actual != expected) {
                        fprintf(stderr, "VIS1 adler32 mismatch: shift=%u pattern=%u len=%lu seed=%08x got=%08x expected=%08x\n",
                                shift, pattern, (unsigned long)len, seeds[s], actual, expected);
                        return 1;
                    }

                    memset(copy, 0xa5, sizeof(copy));
                    uint32_t copied = adler32_copy_vis1(seeds[s], copy + 1, p, len);
                    if (copied != expected || memcmp(copy + 1, p, len) != 0 ||
                        copy[0] != 0xa5 || copy[len + 1] != 0xa5) {
                        fprintf(stderr, "VIS1 adler32_copy mismatch: shift=%u pattern=%u len=%lu\n",
                                shift, pattern, (unsigned long)len);
                        return 1;
                    }
                    ++cases;
                }
            }
        }
    }

    printf("VIS1 adler32 and copy: %u cases passed\n", cases);
    return 0;
}
