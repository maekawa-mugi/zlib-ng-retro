/* Exhaustive mismatch/offset tests for the VIS1 compare256 primitive.
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

uint32_t compare256_vis1(const uint8_t *src0, const uint8_t *src1);

static uint32_t compare_reference(const uint8_t *a, const uint8_t *b) {
    uint32_t i;
    for (i = 0; i < 256; i++) {
        if (a[i] != b[i])
            break;
    }
    return i;
}

int main(void) {
    uint8_t storage_a[272] ALIGNED_(8);
    uint8_t storage_b[272] ALIGNED_(8);
    unsigned int align_a, align_b, mismatch;

#if defined(HAVE_SPARC_GETAUXVAL) && !defined(DISABLE_RUNTIME_CPU_DETECTION)
    /* Avoid calling VIS instructions on an older SPARC machine. */
    if (!sparc_hwcap_has_vis1(getauxval(AT_HWCAP))) {
        puts("VIS1 not available on this CPU: skipping VIS1 instruction test");
        return 77; /* CTest SKIP_RETURN_CODE */
    }
#endif

    for (align_a = 0; align_a < 8; ++align_a) {
        for (align_b = 0; align_b < 8; ++align_b) {
            uint8_t *a = storage_a + align_a;
            uint8_t *b = storage_b + align_b;
            for (mismatch = 0; mismatch <= 256; ++mismatch) {
                /* mismatch == 256 represents a completely equal buffer. */
                for (uint32_t i = 0; i < 256; ++i) {
                    a[i] = (uint8_t)(i * 37u + 13u);
                    b[i] = a[i];
                }
                if (mismatch < 256)
                    b[mismatch] ^= 0x80u;

                uint32_t expected = compare_reference(a, b);
                uint32_t actual = compare256_vis1(a, b);
                if (actual != expected) {
                    fprintf(stderr, "VIS1 mismatch: align=%u,%u diff=%u expected=%u actual=%u\n",
                            align_a, align_b, mismatch, expected, actual);
                    return 1;
                }
            }
        }
    }

    puts("VIS1 compare256: all 16448 alignment/mismatch cases passed");
    return 0;
}
