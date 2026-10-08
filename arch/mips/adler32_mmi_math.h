/* Adler-32 16-byte reduction for PlayStation 2 MMI.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * This is intentionally architecture-neutral, so its arithmetic can be
 * unit-tested with a native host compiler without an R5900 toolchain.
 *
 * On the little-endian EE, PEXTLB/PEXTUB + PADDH yields:
 *   pair[i] = bytes[i] + bytes[i+8], for i=0..7.
 *
 * sum(bytes[0..15]) = sum(pair)
 * weighted16 = sum_{i=0..7} (8-i)*pair[i] + 8*sum(bytes[0..7]).
 * Accumulate sum_{i=0..7}(8-i)*pair[i] using running prefix sums.
 */
#ifndef ADLER32_MMI_MATH_H
#define ADLER32_MMI_MATH_H

#include <stdint.h>

static inline void adler32_mmi_reduce16(uint32_t *s1, uint32_t *s2,
                                         const uint16_t pair[8],
                                         const uint8_t *bytes) {
    uint32_t prefix = 0;
    uint32_t weighted_pairs = 0;
    uint32_t low8 = 0;
    for (unsigned i = 0; i < 8; i++) {
        prefix += pair[i];
        weighted_pairs += prefix;
        low8 += bytes[i];
    }
    *s2 += 16u * (*s1) + weighted_pairs + 8u * low8;
    *s1 += prefix;
}

#endif /* ADLER32_MMI_MATH_H */
