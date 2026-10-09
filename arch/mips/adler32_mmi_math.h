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

static inline void adler32_mmi_reduce16_prefix(uint32_t *s1, uint32_t *s2,
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


/* Algebraically identical 16-byte sum, expressed as explicit coefficients.
 * Benchmark against the prefix recurrence: compilers may schedule these
 * independent terms differently on the R5900. */
static inline void adler32_mmi_reduce16_formula(uint32_t *s1, uint32_t *s2,
                                                  const uint16_t pair[8],
                                                  const uint8_t *bytes) {
    uint32_t total = (uint32_t)pair[0] + pair[1] + pair[2] + pair[3] +
                     pair[4] + pair[5] + pair[6] + pair[7];
    uint32_t weighted = 8u * pair[0] + 7u * pair[1] +
                        6u * pair[2] + 5u * pair[3] +
                        4u * pair[4] + 3u * pair[5] +
                        2u * pair[6] + pair[7];
    uint32_t low8 = (uint32_t)bytes[0] + bytes[1] + bytes[2] + bytes[3] +
                    bytes[4] + bytes[5] + bytes[6] + bytes[7];
    *s2 += 16u * (*s1) + weighted + 8u * low8;
    *s1 += total;
}

/* Retain the original host-test API and original production behavior. */
static inline void adler32_mmi_reduce16(uint32_t *s1, uint32_t *s2,
                                         const uint16_t pair[8],
                                         const uint8_t *bytes) {
    adler32_mmi_reduce16_prefix(s1, s2, pair, bytes);
}

#endif /* ADLER32_MMI_MATH_H */
