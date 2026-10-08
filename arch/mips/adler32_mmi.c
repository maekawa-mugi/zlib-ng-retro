/* Experimental Adler-32 for the PlayStation 2 Emotion Engine.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * The MMI expands 16 bytes to halfwords and computes eight pairwise sums.
 * The positional Adler sum remains exact scalar integer arithmetic; PMADDH
 * is deliberately avoided because it implicitly changes HI/LO registers.
 * Enable only via WITH_MMI_ADLER32, which defaults to OFF.
 */
#ifdef MIPS_MMI_ADLER32
#include "zbuild.h"
#include "arch_functions.h"
#include "adler32_p.h"
#include "adler32_mmi_math.h"

Z_INTERNAL uint32_t adler32_mmi(uint32_t adler, const uint8_t *buf, size_t len) {
    uint32_t a = adler & 0xffffu, b = adler >> 16;
    if (len < 16)
        return adler32_c(adler, buf, len);

    while (len != 0) {
        size_t n = MIN(len, (size_t)NMAX);
        len -= n;

        /* Keep each 32-bit sum in range; NMAX is the zlib Adler bound. */
        while (n != 0 && ((uintptr_t)buf & 15u) != 0) {
            a += *buf++;
            b += a;
            n--;
        }
        while (n >= 16) {
            uint16_t pairs[8] ALIGNED_(16);
            /* Both buf and pairs are 16-byte aligned (LQ/SQ mask low 4 bits).
             * On little-endian EE, lane i contains buf[i] + buf[8+i]. */
            __asm__ volatile (
                "lq     $8, 0(%[src])\n\t"
                "pextlb $9, $0, $8\n\t"
                "pextub $10, $0, $8\n\t"
                "paddh  $9, $9, $10\n\t"
                "sq     $9, 0(%[pairs])"
                :
                : [src] "r" (buf), [pairs] "r" (pairs)
                : "$8", "$9", "$10", "memory"
            );

            /* The first eight bytes and eight pair sums suffice for
             * the exact positional sum. No 16-element multiply loop. */
            adler32_mmi_reduce16(&a, &b, pairs, buf);
            buf += 16;
            n -= 16;
        }
        while (n-- != 0) {
            a += *buf++;
            b += a;
        }

        a %= BASE;
        b %= BASE;
    }
    return (b << 16) | a;
}

/* Baseline two-pass copy. Preserved for reproducible A/B measurements. */
Z_INTERNAL uint32_t adler32_copy_mmi_twopass(uint32_t adler, uint8_t *dst,
                                              const uint8_t *src, size_t len) {
    uint32_t result = adler32_mmi(adler, src, len);
    memcpy(dst, src, len);
    return result;
}

/* A single input traversal combines Adler accumulation and output copying.
 * 16-byte SQ copying is used only for an aligned destination; unaligned
 * destinations are copied with memcpy after the input block was loaded.
 * This follows the same non-overlapping memcpy contract as the baseline.
 */
Z_INTERNAL uint32_t adler32_copy_mmi_fused(uint32_t adler, uint8_t *dst,
                                           const uint8_t *src, size_t len) {
    if (len < 16) {
        uint32_t result = adler32_c(adler, src, len);
        if (len != 0 && dst != src)
            memcpy(dst, src, len);
        return result;
    }

    uint32_t a = adler & 0xffffu, b = adler >> 16;
    while (len != 0) {
        size_t n = MIN(len, (size_t)NMAX);
        len -= n;

        while (n != 0 && ((uintptr_t)src & 15u) != 0) {
            uint8_t value = *src++;
            a += value;
            b += a;
            *dst++ = value;
            --n;
        }

        while (n >= 16) {
            uint16_t pairs[8] ALIGNED_(16);
            if (((uintptr_t)dst & 15u) == 0) {
                __asm__ volatile (
                    "lq     $8, 0(%[src])\n\t"
                    "pextlb $9, $0, $8\n\t"
                    "pextub $10, $0, $8\n\t"
                    "paddh  $9, $9, $10\n\t"
                    "sq     $9, 0(%[pairs])\n\t"
                    "sq     $8, 0(%[dst])"
                    :
                    : [src] "r" (src), [pairs] "r" (pairs), [dst] "r" (dst)
                    : "$8", "$9", "$10", "memory"
                );
            } else {
                __asm__ volatile (
                    "lq     $8, 0(%[src])\n\t"
                    "pextlb $9, $0, $8\n\t"
                    "pextub $10, $0, $8\n\t"
                    "paddh  $9, $9, $10\n\t"
                    "sq     $9, 0(%[pairs])"
                    :
                    : [src] "r" (src), [pairs] "r" (pairs)
                    : "$8", "$9", "$10", "memory"
                );
            }

            adler32_mmi_reduce16(&a, &b, pairs, src);
            if (((uintptr_t)dst & 15u) != 0 && dst != src)
                memcpy(dst, src, 16);
            src += 16;
            dst += 16;
            n -= 16;
        }

        while (n-- != 0) {
            uint8_t value = *src++;
            a += value;
            b += a;
            *dst++ = value;
        }
        a %= BASE;
        b %= BASE;
    }
    return (b << 16) | a;
}

Z_INTERNAL uint32_t adler32_copy_mmi(uint32_t adler, uint8_t *dst,
                                     const uint8_t *src, size_t len) {
#ifdef MIPS_MMI_ADLER32_FUSED_COPY
    return adler32_copy_mmi_fused(adler, dst, src, len);
#else
    return adler32_copy_mmi_twopass(adler, dst, src, len);
#endif
}
#endif /* MIPS_MMI_ADLER32 */
