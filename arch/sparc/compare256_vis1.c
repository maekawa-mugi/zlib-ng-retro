/* compare256_vis1.c -- 256-byte compare using SPARC VIS1 fcmpeq8.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "arch_functions.h"

#ifdef SPARC_VIS1

/* VIS1 fcmpeq8 compares 8 byte lanes and writes an 8-bit equality mask.
 * SPARC ldd requires 8-byte alignment. Explicitly clobber the FP
 * registers used to hold the loaded data. */
static inline uint32_t vis1_equal8(const uint8_t *a, const uint8_t *b) {
    uint32_t mask;
    __asm__ __volatile__(
        "ldd [%1], %%f0\n\t"
        "ldd [%2], %%f2\n\t"
        "fcmpeq8 %%f0, %%f2, %0"
        : "=r" (mask)
        : "r" (a), "r" (b)
        : "f0", "f1", "f2", "f3", "memory");
    return mask;
}

Z_INTERNAL uint32_t compare256_vis1(const uint8_t *src0, const uint8_t *src1) {
    uint32_t n = 0;

    /* Different pointer alignments cannot simultaneously become 8-aligned.
     * Handle these accesses bytewise; never issue an unaligned ldd. */
    if ((((uintptr_t)src0 ^ (uintptr_t)src1) & 7u) != 0u) {
        for (; n < 256; ++n) {
            if (src0[n] != src1[n])
                return n;
        }
        return 256;
    }

    /* Match common alignment before issuing VIS loads. */
    for (; n < 256 && (((uintptr_t)(src0 + n) & 7u) != 0u); ++n) {
        if (src0[n] != src1[n])
            return n;
    }

    /* Inspect the first unequal block in byte order to return exactly the
     * generic match length, independently of mask bit ordering. */
    for (; n + 8 <= 256; n += 8) {
        if (vis1_equal8(src0 + n, src1 + n) != 0xffu) {
            for (uint32_t i = 0; i < 8; ++i) {
                if (src0[n + i] != src1[n + i])
                    return n + i;
            }
        }
    }

    for (; n < 256; ++n) {
        if (src0[n] != src1[n])
            return n;
    }
    return 256;
}
/* The compression hot path calls longest_match directly, so specialize all
 * three match implementations to use the VIS1 comparison primitive too.
 * Keep the generic implementations for machines without VIS1. */
#define COMPARE256 compare256_vis1

#define LONGEST_MATCH longest_match_vis1
#include "match_tpl.h"

#define LONGEST_MATCH_SLOW
#define LONGEST_MATCH longest_match_slow_knuth_vis1
#include "match_tpl.h"

#define LONGEST_MATCH_SLOW
#define LONGEST_MATCH_SLOW_ROLL
#define LONGEST_MATCH longest_match_slow_roll_vis1
#include "match_tpl.h"

#endif /* SPARC_VIS1 */
