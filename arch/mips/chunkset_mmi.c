/* chunkset_mmi.c -- conservative R5900 MMI inflate back-reference copying.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Only the common forward-copy case with a distance of at least 16 bytes
 * and matching alignment residues uses LQ/SQ. In particular, LZ77 copies
 * with shorter distances MUST observe newly written bytes, and copies from
 * ahead of the destination may need memmove semantics. Those cases use the
 * generic proven implementation instead.
 */
#ifdef MIPS_MMI

#include "zbuild.h"
#include "arch_functions.h"

static inline uint8_t *chunkmemset_safe_mmi_impl(uint8_t *out, uint8_t *from,
                                                  size_t len, size_t left,
                                                  int burst) {
    size_t n = MIN(len, left);
    uint8_t *dst = out;
    const uint8_t *src = from;

    if (n == 0)
        return out;

    /* Use uintptr_t, not unrelated-pointer subtraction, for the distance.
     * The caller guarantees a valid source span for the requested copy.
     * The scalar function deals with short distances, forward sources,
     * different alignment residues and small copies. */
    if (n < 32 || (uintptr_t)src >= (uintptr_t)dst ||
        ((uintptr_t)dst - (uintptr_t)src) < 16 ||
        (((uintptr_t)dst ^ (uintptr_t)src) & 15u) != 0)
        return chunkmemset_safe_c(out, from, len, left);

    /* Scalar prefix brings both addresses to a 16-byte boundary. */
    while (((uintptr_t)dst & 15u) != 0 && n != 0) {
        *dst++ = *src++;
        --n;
    }

    /* The 4-load burst is safe only if the history distance is at least
     * 64: for distance 16/32/48, later loads must see earlier writes.
     * Keep the serial schedule as the baseline and for close overlaps. */
    const uintptr_t distance = (uintptr_t)out - (uintptr_t)from;
    while (n >= 64) {
        if (burst && distance >= 64) {
            __asm__ volatile (
                "lq $8, 0(%[src])\n\t"
                "lq $9, 16(%[src])\n\t"
                "lq $10, 32(%[src])\n\t"
                "lq $11, 48(%[src])\n\t"
                "sq $8, 0(%[dst])\n\t"
                "sq $9, 16(%[dst])\n\t"
                "sq $10, 32(%[dst])\n\t"
                "sq $11, 48(%[dst])"
                :
                : [src] "r" (src), [dst] "r" (dst)
                : "$8", "$9", "$10", "$11", "memory"
            );
        } else {
            __asm__ volatile (
                "lq $8, 0(%[src])\n\t"
                "sq $8, 0(%[dst])\n\t"
                "lq $8, 16(%[src])\n\t"
                "sq $8, 16(%[dst])\n\t"
                "lq $8, 32(%[src])\n\t"
                "sq $8, 32(%[dst])\n\t"
                "lq $8, 48(%[src])\n\t"
                "sq $8, 48(%[dst])"
                :
                : [src] "r" (src), [dst] "r" (dst)
                : "$8", "memory"
            );
        }
        dst += 64;
        src += 64;
        n -= 64;
    }

    while (n >= 16) {
        __asm__ volatile (
            "lq $8, 0(%[src])\n\t"
            "sq $8, 0(%[dst])"
            :
            : [src] "r" (src), [dst] "r" (dst)
            : "$8", "memory"
        );
        dst += 16;
        src += 16;
        n -= 16;
    }

    while (n-- != 0)
        *dst++ = *src++;

    return dst;
}

/* Both functions remain linked for same-run correctness and timing tests.
 * Production dispatch uses serial unless the experiment is selected. */
Z_INTERNAL uint8_t *chunkmemset_safe_mmi_serial(uint8_t *out, uint8_t *from,
                                                size_t len, size_t left) {
    return chunkmemset_safe_mmi_impl(out, from, len, left, 0);
}

Z_INTERNAL uint8_t *chunkmemset_safe_mmi_burst(uint8_t *out, uint8_t *from,
                                               size_t len, size_t left) {
    return chunkmemset_safe_mmi_impl(out, from, len, left, 1);
}

/* Very short LZ77 history distances 1/2/4/8 are periodic with a
 * 16-byte quadword. Build one exact period after scalar alignment peel,
 * then emit independent SQ stores. The baseline generic copy handles
 * small lengths, other distances and forward-source semantics.
 *
 * This is an experimental throughput candidate, not a replacement for
 * the safety conditions in chunkmemset_safe_mmi_impl().
 */
static uint8_t *chunkmemset_safe_mmi_period_impl(uint8_t *out, uint8_t *from,
                                                  size_t len, size_t left, int wide) {
    size_t n = MIN(len, left);
    uintptr_t from_addr = (uintptr_t)from;
    uintptr_t out_addr = (uintptr_t)out;
    size_t distance = out_addr > from_addr ? (size_t)(out_addr - from_addr) : 0;
    if (n >= 64 && (distance == 1 || distance == 2 ||
                    distance == 4 || distance == 8)) {
        uint8_t *dst = out;
        const uint8_t *src = from;

        /* This peel must respect forward sequential LZ77 overlap. */
        while (n != 0 && ((uintptr_t)dst & 15u) != 0) {
            *dst++ = *src++;
            --n;
        }
        if (n >= 16) {
            uint8_t period[16] ALIGNED_(16);
            const uint8_t *last = dst - distance;
            for (unsigned i = 0; i < 16; ++i)
                period[i] = last[i % distance];

            /* Eight SQs per one LQ, kept resident in an MMI GPR to
             * amortize load-use interlocks. Benchmark against 4 SQ. */
            while (wide && n >= 128) {
                __asm__ volatile (
                    "lq $8, 0(%[period])\n\t"
                    "sq $8, 0(%[dst])\n\t"
                    "sq $8, 16(%[dst])\n\t"
                    "sq $8, 32(%[dst])\n\t"
                    "sq $8, 48(%[dst])\n\t"
                    "sq $8, 64(%[dst])\n\t"
                    "sq $8, 80(%[dst])\n\t"
                    "sq $8, 96(%[dst])\n\t"
                    "sq $8, 112(%[dst])"
                    : : [period] "r" (period), [dst] "r" (dst)
                    : "$8", "memory"
                );
                dst += 128;
                n -= 128;
            }
            while (n >= 64) {
                __asm__ volatile (
                    "lq $8, 0(%[period])\n\t"
                    "sq $8, 0(%[dst])\n\t"
                    "sq $8, 16(%[dst])\n\t"
                    "sq $8, 32(%[dst])\n\t"
                    "sq $8, 48(%[dst])"
                    :
                    : [period] "r" (period), [dst] "r" (dst)
                    : "$8", "memory"
                );
                dst += 64;
                n -= 64;
            }
            while (n >= 16) {
                __asm__ volatile (
                    "lq $8, 0(%[period])\n\t"
                    "sq $8, 0(%[dst])"
                    :
                    : [period] "r" (period), [dst] "r" (dst)
                    : "$8", "memory"
                );
                dst += 16;
                n -= 16;
            }
        }
        while (n-- != 0) {
            *dst = *(dst - distance);
            dst++;
        }
        return dst;
    }
#ifdef MIPS_MMI_CHUNKSET_BURST
    return chunkmemset_safe_mmi_burst(out, from, len, left);
#else
    return chunkmemset_safe_mmi_serial(out, from, len, left);
#endif
}

Z_INTERNAL uint8_t *chunkmemset_safe_mmi_pattern(uint8_t *out, uint8_t *from,
                                                 size_t len, size_t left) {
    return chunkmemset_safe_mmi_period_impl(out, from, len, left, 0);
}
Z_INTERNAL uint8_t *chunkmemset_safe_mmi_pattern128(uint8_t *out, uint8_t *from,
                                                    size_t len, size_t left) {
    return chunkmemset_safe_mmi_period_impl(out, from, len, left, 1);
}

Z_INTERNAL uint8_t *chunkmemset_safe_mmi(uint8_t *out, uint8_t *from,
                                         size_t len, size_t left) {
#ifdef MIPS_MMI_CHUNKSET_PATTERN128
    return chunkmemset_safe_mmi_pattern128(out, from, len, left);
#elif defined(MIPS_MMI_CHUNKSET_PATTERN)
    return chunkmemset_safe_mmi_pattern(out, from, len, left);
#elif defined(MIPS_MMI_CHUNKSET_BURST)
    return chunkmemset_safe_mmi_burst(out, from, len, left);
#else
    /* EE quick benchmarks: C wins for LZ77 distance 1/2/4/8, while
     * serial LQ/SQ wins for distance >=64. Do not assume the unmeasured
     * 9..63-byte region also benefits from MMI. For small lengths the
     * dispatch and scalar-peel overhead exceeds a useful vector batch. */
    const uintptr_t dst = (uintptr_t)out;
    const uintptr_t src = (uintptr_t)from;
    if (dst <= src || dst - src < 64u || MIN(len, left) < 64u)
        return chunkmemset_safe_c(out, from, len, left);
    return chunkmemset_safe_mmi_serial(out, from, len, left);
#endif
}
#endif /* MIPS_MMI */
