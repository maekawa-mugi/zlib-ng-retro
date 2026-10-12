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

/* Below this length the generic C path is faster than the vector kernel. */
#ifndef ADLER32_MMI_VEC_MIN
#  define ADLER32_MMI_VEC_MIN 32
#endif

static uint32_t adler32_mmi_impl(uint32_t adler, const uint8_t *buf,
                                 size_t len, int formula) {
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
            if (formula)
                adler32_mmi_reduce16_formula(&a, &b, pairs, buf);
            else
                adler32_mmi_reduce16_prefix(&a, &b, pairs, buf);
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

Z_INTERNAL uint32_t adler32_mmi_prefix(uint32_t adler, const uint8_t *buf,
                                        size_t len) {
    return adler32_mmi_impl(adler, buf, len, 0);
}

Z_INTERNAL uint32_t adler32_mmi_formula(uint32_t adler, const uint8_t *buf,
                                         size_t len) {
    return adler32_mmi_impl(adler, buf, len, 1);
}

uint32_t adler32_mmi_vec(uint32_t adler, const uint8_t *buf, size_t len);

/* Production entry: the multiplier-free vector kernel. The older prefix and
 * formula reductions stay selectable for A/B comparison only. */
Z_INTERNAL uint32_t adler32_mmi(uint32_t adler, const uint8_t *buf, size_t len) {
#ifdef MIPS_MMI_ADLER32_FORMULA
    return adler32_mmi_formula(adler, buf, len);
#else
    return adler32_mmi_vec(adler, buf, len);
#endif
}

/* ---------------------------------------------------------------------
 * Vector Adler-32 ("vec"): multiplier-free prefix-sum kernel written as a
 * stand-alone assembly function; see adler32_mmi_vec.h for the derivation.
 * Windows of at most 346 quadwords keep the exact sums below 2^32.
 * ------------------------------------------------------------------ */
#include "adler32_mmi_vec.h"

uint32_t adler32_mmi_vec_kernel(uint32_t adler, const uint8_t *p16, size_t nq,
                                const uint8_t *head_mask, const uint8_t *tail_mask,
                                size_t head_off, const uint8_t *mask_base);

/* 16 tail masks (keep the first 16-pad bytes) then 16 head masks (keep the
 * bytes from offset off on). Entry 0 of the tail table is all ones. */
static const uint8_t adler_vec_masks[512] ALIGNED_(16) = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
};

Z_INTERNAL uint32_t adler32_mmi_vec(uint32_t adler, const uint8_t *buf, size_t len) {
    if (len < ADLER32_MMI_VEC_MIN)
        return adler32_c(adler, buf, len);
    const size_t off = (uintptr_t)buf & 15u;
    const uint8_t *p = buf - off;
    size_t rem = len + off;
    size_t head = off;
    for (;;) {
        size_t nq = (rem + 15) >> 4;
        if (nq > 346)
            nq = 346;
        const size_t span = nq << 4;
        const size_t pad = span >= rem ? span - rem : 0;
        adler = adler32_mmi_vec_kernel(adler, p, nq,
                                       adler_vec_masks + 256 + 16 * head,
                                       adler_vec_masks + 16 * pad,
                                       head, adler_vec_masks);
        if (span >= rem)
            return adler;
        p += span;
        rem -= span;
        head = 0;
    }
}

void *zng_mmi_memcpy(void *dst, const void *src, size_t n);

/* Vector checksum plus the platform libc memcpy (reference for A/B). */
Z_INTERNAL uint32_t adler32_copy_mmi_vec_libc(uint32_t adler, uint8_t *dst,
                                               const uint8_t *src, size_t len) {
    uint32_t result = adler32_mmi_vec(adler, src, len);
    memcpy(dst, src, len);
    return result;
}

/* Vector checksum plus the QFSRV copy engine (exact Adler, any alignment). */
Z_INTERNAL uint32_t adler32_copy_mmi_vec_twopass(uint32_t adler, uint8_t *dst,
                                                  const uint8_t *src, size_t len) {
    uint32_t result = adler32_mmi_vec(adler, src, len);
    zng_mmi_memcpy(dst, src, len);
    return result;
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

#ifdef MIPS_MMI_ADLER32_FORMULA
            adler32_mmi_reduce16_formula(&a, &b, pairs, src);
#else
            adler32_mmi_reduce16_prefix(&a, &b, pairs, src);
#endif
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
    return adler32_copy_mmi_vec_twopass(adler, dst, src, len);
#endif
}
#endif /* MIPS_MMI_ADLER32 */
