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

    while (len < 256) {
        if ((((uintptr_t)(src0 + len) | (uintptr_t)(src1 + len)) & 15u) == 0) {
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

            /* A mismatch must be reported at the first BYTE, not the
             * first halfword/word. Reading the output as bytes is also
             * independent of integer byte order. */
            for (unsigned i = 0; i < 16; i++)
                if (differences[i] != 0)
                    return len + i;
            len += 16;
        } else {
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
