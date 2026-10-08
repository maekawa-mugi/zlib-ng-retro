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

Z_INTERNAL uint8_t *chunkmemset_safe_mmi(uint8_t *out, uint8_t *from,
                                         size_t len, size_t left) {
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

    /* Interleave loads and stores. Loading all four input blocks before
     * storing them would break the distance-16 overlapping case. */
    while (n >= 64) {
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
#endif /* MIPS_MMI */
