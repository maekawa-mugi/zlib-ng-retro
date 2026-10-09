/* Independent scalar and generic-C comparison for experimental MMI Adler-32.
 * Requires WITH_MMI_ADLER32=ON; run the executable on a PS2 EE. */
#include "zbuild.h"
#include "arch_functions.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifndef MIPS_MMI_ADLER32
#  error "Build with WITH_MMI_ADLER32=ON"
#endif
static uint8_t src[65536 + 32] ALIGNED_(16);
static uint8_t dst[65536 + 32] ALIGNED_(16);

/* Independent scalar oracle; 64-bit sums avoid NMAX/alignment overflow
 * even for the maximum 64KiB input used by this test. */
static uint32_t reference(uint32_t initial, const uint8_t *data, size_t n) {
    uint64_t a = initial & 65535u, b = initial >> 16;
    for (size_t i = 0; i < n; ++i) {
        a += data[i];
        b += a;
    }
    /* A non-NULL empty input also normalizes both initial components. */
    return (uint32_t)((b % 65521u) << 16) | (uint32_t)(a % 65521u);
}

static int run_case(size_t n, unsigned off, uint32_t initial) {
    const uint8_t *data = src + off;
    uint32_t expected = reference(initial, data, n);
    uint32_t generic = adler32_c(initial, data, n);
    if (generic != expected) {
        printf("Generic Adler FAIL len=%lu offset=%u initial=%08lx got=%08lx expected=%08lx\n",
               (unsigned long)n, off, (unsigned long)initial,
               (unsigned long)generic, (unsigned long)expected);
        return 1;
    }
    uint32_t actual = adler32_mmi(initial, data, n);
    uint32_t prefix = adler32_mmi_prefix(initial, data, n);
    uint32_t formula = adler32_mmi_formula(initial, data, n);
    if (actual != expected || prefix != expected || formula != expected) {
        printf("MMI Adler FAIL len=%lu offset=%u initial=%08lx got=%08lx prefix=%08lx formula=%08lx expected=%08lx\n",
               (unsigned long)n, off, (unsigned long)initial,
               (unsigned long)actual, (unsigned long)prefix,
               (unsigned long)formula, (unsigned long)expected);
        return 1;
    }
    /* Destination misalignment can differ from the source residue.
     * Check both copy schedules, production dispatch and output sentinels. */
    for (unsigned destoff = 0; destoff < 16; destoff += 5) {
        for (unsigned variant = 0; variant < 3; ++variant) {
            memset(dst, 0xa5, sizeof(dst));
            uint8_t *output = dst + destoff;
            if (variant == 0)
                actual = adler32_copy_mmi(initial, output, data, n);
            else if (variant == 1)
                actual = adler32_copy_mmi_twopass(initial, output, data, n);
            else
                actual = adler32_copy_mmi_fused(initial, output, data, n);
            if (actual != expected || memcmp(output, data, n) != 0 ||
                (n + destoff < sizeof(dst) && output[n] != 0xa5u) ||
                (destoff != 0 && dst[destoff - 1] != 0xa5u)) {
                printf("MMI Adler copy FAIL len=%lu srcoff=%u dstoff=%u variant=%u\n",
                       (unsigned long)n, off, destoff, variant);
                return 1;
            }
        }
    }
    size_t first = n / 3;
    actual = adler32_mmi(initial, data, first);
    actual = adler32_mmi(actual, data + first, n - first);
    if (actual != expected) {
        printf("MMI Adler split FAIL len=%lu offset=%u\n", (unsigned long)n, off);
        return 1;
    }
    return 0;
}
int main(void) {
    static const size_t lengths[] = {
        0, 1, 2, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64,
        127, 128, 255, 256, 511, 512, 1023, 1024, 2047,
        4095, 5551, 5552, 5553, 8192, 16384, 32767, 65536
    };
    static const uint32_t initial[] = {
        1u, 0u, 0xffffffffu, 0x00010001u, 0xabcdef01u
    };
    uint32_t rng = 0x9e3779b9u;
    for (unsigned pattern = 0; pattern < 3; pattern++) {
        for (unsigned i = 0; i < sizeof(src); i++) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            src[i] = pattern == 0 ? (uint8_t)rng :
                     pattern == 1 ? 0xffu : (uint8_t)(i & 15u);
        }
        for (unsigned off = 0; off < 16; off++)
            for (unsigned l = 0; l < sizeof(lengths)/sizeof(lengths[0]); l++)
                for (unsigned j = 0; j < sizeof(initial)/sizeof(initial[0]); j++)
                    if (run_case(lengths[l], off, initial[j]))
                        return 1;
    }
    puts("MMI Adler-32: PASS");
    return 0;
}
