/* compare256_mmi.c -- R5900 MMI 128-bit match-length comparison.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * LQ rounds unaligned addresses down, so vector loads are ONLY used when
 * BOTH input pointers are 16-byte aligned.  All other bytes use the scalar
 * path, including the case where their alignment residues differ.
 */
#ifdef MIPS_MMI

#include "zbuild.h"
#include "arch_functions.h"
#include "zendian.h"
#include "deflate.h"
#include "fallback_builtins.h"

Z_INTERNAL uint32_t compare256_mmi(const uint8_t *src0, const uint8_t *src1) {
    uint8_t differences[16] ALIGNED_(16);
    uint32_t len = 0;
    /* Different 16-byte alignment residues can never both reach an
     * aligned address at the same comparison offset. Use safe 64-bit
     * SWAR comparisons instead of falling back to 256 byte checks. */
    const int same_alignment = (((uintptr_t)src0 ^ (uintptr_t)src1) & 15u) == 0;

    while (len < 256) {
        if (same_alignment && len <= 240 &&
            (((uintptr_t)(src0 + len) & 15u) == 0)) {
            /* Keep all three addresses aligned: EE's LQ/SQ mask low bits. */
            __asm__ volatile (
                "lq   $8, 0(%[a])\n\t"
                "lq   $9, 0(%[b])\n\t"
                "pxor $8, $8, $9\n\t"
                "sq   $8, 0(%[out])"
                :
                : [a] "r" (src0 + len), [b] "r" (src1 + len),
                  [out] "r" (differences)
                : "$8", "$9", "memory"
            );

            /* Fast all-equal check, with byte-wise search only on
             * mismatch. This is independent of integer byte order. */
            uint64_t lo = zng_memread_8(differences);
            uint64_t hi = zng_memread_8(differences + 8);
            if ((lo | hi) != 0) {
                for (unsigned i = 0; i < 16; i++)
                    if (differences[i] != 0)
                        return len + i;
            }
            len += 16;
        } else if (!same_alignment && len <= 248) {
            /* Different residues cannot be vectorized with LQ.
             * Safe unaligned 8-byte reads preserve comparison speed. */
            uint64_t diff = zng_memread_8(src0 + len) ^
                            zng_memread_8(src1 + len);
            if (diff != 0)
                return len + zng_first_diff_byte64(diff);
            len += 8;
        } else {
            /* Scalar peel to the next common 16-byte boundary. */
            if (src0[len] != src1[len])
                return len;
            len++;
        }
    }
    return 256;
}

/* Replace the three longest-match variants as well, so the comparison is
 * actually used by deflate's hot path, not only by a standalone caller. */
#define COMPARE256 compare256_mmi
#define LONGEST_MATCH longest_match_mmi
#include "match_tpl.h"

#define LONGEST_MATCH_SLOW
#define LONGEST_MATCH longest_match_slow_knuth_mmi
#include "match_tpl.h"

#define LONGEST_MATCH_SLOW
#define LONGEST_MATCH_SLOW_ROLL
#define LONGEST_MATCH longest_match_slow_roll_mmi
#include "match_tpl.h"

#endif /* MIPS_MMI */
