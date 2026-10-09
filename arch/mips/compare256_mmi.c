/* compare256_mmi.c -- R5900 MMI 128-bit match-length comparison.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * LQ rounds unaligned addresses down, so vector loads are ONLY used when
 * BOTH input pointers are 16-byte aligned. Other alignments use scalar
 * peeling or safe unaligned 8-byte reads. Optional 64-byte prefilter
 * uses PXOR/POR to skip four matching vectors with one result store.
 */
#ifdef MIPS_MMI

#include "zbuild.h"
#include "arch_functions.h"
#include "zendian.h"
#include "deflate.h"
#include "fallback_builtins.h"

static inline uint32_t compare256_mmi_impl(const uint8_t *src0,
                                           const uint8_t *src1,
                                           int prefilter64, int firstdiff64, int hybrid16) {
#ifndef MIPS_MMI_COMPARE64
    (void)prefilter64;
#endif
    uint8_t differences[16] ALIGNED_(16);
    uint32_t len = 0;
    /* Early mismatches dominate many Deflate candidate probes.
     * Two safe unaligned 64-bit XORs can reject their first 16 bytes
     * without entering the MMI store/reload/branch path. Aligned
     * long matches continue with the original full MMI vector loop. */
    if (hybrid16) {
        uint64_t lo=zng_memread_8(src0)^zng_memread_8(src1);
        uint64_t hi;
        if (lo!=0) return zng_first_diff_byte64(lo);
        hi=zng_memread_8(src0+8)^zng_memread_8(src1+8);
        if (hi!=0) return 8U+zng_first_diff_byte64(hi);
        len=16U;
    }
    /* Different 16-byte alignment residues can never both reach an
     * aligned address at the same comparison offset. Use safe 64-bit
     * SWAR comparisons instead of falling back to 256 byte checks. */
    const int same_alignment = (((uintptr_t)src0 ^ (uintptr_t)src1) & 15u) == 0;

    while (len < 256) {
#ifdef MIPS_MMI_COMPARE64
        /* Fast reject/accept for 64 matching bytes using four independent
         * 128-bit XORs reduced with POR. If any byte differs, fall back
         * to the existing 16-byte path to locate the first mismatch.
         * len<=192 guarantees that the full 64-byte load is in bounds. */
        if (prefilter64 == 1 && same_alignment && len <= 192 &&
            (((uintptr_t)(src0 + len) & 15u) == 0)) {
            __asm__ volatile (
                "lq   $8, 0(%[a])\n\t"
                "lq   $9, 0(%[b])\n\t"
                "pxor $10, $8, $9\n\t"
                "lq   $8, 16(%[a])\n\t"
                "lq   $9, 16(%[b])\n\t"
                "pxor $8, $8, $9\n\t"
                "por  $10, $10, $8\n\t"
                "lq   $8, 32(%[a])\n\t"
                "lq   $9, 32(%[b])\n\t"
                "pxor $8, $8, $9\n\t"
                "por  $10, $10, $8\n\t"
                "lq   $8, 48(%[a])\n\t"
                "lq   $9, 48(%[b])\n\t"
                "pxor $8, $8, $9\n\t"
                "por  $10, $10, $8\n\t"
                "sq   $10, 0(%[out])"
                :
                : [a] "r" (src0 + len), [b] "r" (src1 + len),
                  [out] "r" (differences)
                : "$8", "$9", "$10", "memory"
            );
            if ((zng_memread_8(differences) |
                 zng_memread_8(differences + 8)) == 0) {
                len += 64;
                continue;
            }
        }
#endif
        /* Two-quadword variant tests whether a shorter dependency chain is
         * preferable to four quadwords on EE's small 8KiB data cache. */
        if (prefilter64 == 2 && same_alignment && len <= 224 &&
            (((uintptr_t)(src0 + len) & 15u) == 0)) {
            __asm__ volatile (
                "lq   $8, 0(%[a])\n\t"
                "lq   $9, 0(%[b])\n\t"
                "lq   $10, 16(%[a])\n\t"
                "lq   $11, 16(%[b])\n\t"
                "pxor $8, $8, $9\n\t"
                "pxor $10, $10, $11\n\t"
                "por  $8, $8, $10\n\t"
                "sq   $8, 0(%[out])"
                :
                : [a] "r" (src0 + len), [b] "r" (src1 + len),
                  [out] "r" (differences)
                : "$8", "$9", "$10", "$11", "memory"
            );
            if ((zng_memread_8(differences) |
                 zng_memread_8(differences + 8)) == 0) {
                len += 32;
                continue;
            }
        }
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
                if (firstdiff64) {
                    /* Read back the same two 64-bit words already used for
                     * the all-equal test. Reuse the endian-independent SWAR
                     * first-mismatch helper used by the unaligned fallback.
                     * Avoid sixteen dependent bytewise comparisons here. */
                    if (lo != 0)
                        return len + zng_first_diff_byte64(lo);
                    return len + 8 + zng_first_diff_byte64(hi);
                }
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

/* Keep the original bytewise mismatch search and its SWAR alternative
 * callable in the same EE executable. The optional 64-byte equality
 * prefilter is independent of this choice, giving four A/B combinations. */
Z_INTERNAL uint32_t compare256_mmi_plain(const uint8_t *a, const uint8_t *b) {
    return compare256_mmi_impl(a, b, 0, 0, 0);
}
Z_INTERNAL uint32_t compare256_mmi_swar(const uint8_t *a, const uint8_t *b) {
    return compare256_mmi_impl(a, b, 0, 1, 0);
}
Z_INTERNAL uint32_t compare256_mmi_hybrid16(const uint8_t *a, const uint8_t *b) {
    return compare256_mmi_impl(a, b, 0, 1, 1);
}

#ifdef MIPS_MMI_COMPARE64
Z_INTERNAL uint32_t compare256_mmi_hybrid16_pre64(const uint8_t *a, const uint8_t *b) {
    return compare256_mmi_impl(a, b, 1, 1, 1);
}
Z_INTERNAL uint32_t compare256_mmi_prefilter32(const uint8_t *a, const uint8_t *b) {
    return compare256_mmi_impl(a, b, 2, 0, 0);
}
Z_INTERNAL uint32_t compare256_mmi_prefilter32_swar(const uint8_t *a, const uint8_t *b) {
    return compare256_mmi_impl(a, b, 2, 1, 0);
}
#endif

#ifdef MIPS_MMI_COMPARE64
Z_INTERNAL uint32_t compare256_mmi_prefilter64(const uint8_t *a, const uint8_t *b) {
    return compare256_mmi_impl(a, b, 1, 0, 0);
}
Z_INTERNAL uint32_t compare256_mmi_prefilter64_swar(const uint8_t *a, const uint8_t *b) {
    return compare256_mmi_impl(a, b, 1, 1, 0);
}
#endif

Z_INTERNAL uint32_t compare256_mmi(const uint8_t *a, const uint8_t *b) {
#ifdef MIPS_MMI_COMPARE64
#  ifdef MIPS_MMI_COMPARE32
#    ifdef MIPS_MMI_COMPARE_SWAR
    return compare256_mmi_prefilter32_swar(a, b);
#    else
    return compare256_mmi_prefilter32(a, b);
#    endif
#  elif defined(MIPS_MMI_COMPARE_SWAR)
    return compare256_mmi_prefilter64_swar(a, b);
#  else
    return compare256_mmi_prefilter64(a, b);
#  endif
#else
#  ifdef MIPS_MMI_COMPARE_SWAR
    return compare256_mmi_swar(a, b);
#  else
    return compare256_mmi_plain(a, b);
#  endif
#endif
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
