/* chunkmemset_sparc.c -- conservative aligned SPARC FP-register copy path.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "arch_functions.h"

#ifdef SPARC_VIS1_CHUNKSET

/* This optimization uses ordinary SPARC FP 64-bit ldd/std pairs, available
 * on VIS1 machines. It intentionally does not use VIS-specific alignment
 * state (GSR). The original generic implementation handles small distances,
 * different source/destination alignments and forward source pointers.
 *
 * The loads and stores MUST be interleaved: for LZ77 distances of 8, 16,
 * or 24 bytes, later loads depend on bytes written by earlier stores. */
Z_INTERNAL uint8_t *chunkmemset_safe_sparc(uint8_t *out, uint8_t *from,
                                           size_t len, size_t left) {
    size_t n = MIN(len, left);
    uintptr_t dst_addr = (uintptr_t)out;
    uintptr_t src_addr = (uintptr_t)from;

    if (n < 32 || dst_addr <= src_addr || dst_addr - src_addr < 8 ||
        ((dst_addr ^ src_addr) & 7u) != 0u)
        return chunkmemset_safe_c(out, from, len, left);

    /* Both pointers can be aligned together, because their low 3 bits
     * match. Scalar prefix copies are required for SPARC ldd/std. */
    while (n && ((uintptr_t)out & 7u)) {
        *out++ = *from++;
        --n;
    }

    while (n >= 32) {
        __asm__ __volatile__(
            "ldd [%1], %%f0\n\t"
            "std %%f0, [%0]\n\t"
            "ldd [%1 + 8], %%f2\n\t"
            "std %%f2, [%0 + 8]\n\t"
            "ldd [%1 + 16], %%f4\n\t"
            "std %%f4, [%0 + 16]\n\t"
            "ldd [%1 + 24], %%f6\n\t"
            "std %%f6, [%0 + 24]"
            :
            : "r" (out), "r" (from)
            : "memory", "f0", "f1", "f2", "f3",
              "f4", "f5", "f6", "f7");
        out += 32;
        from += 32;
        n -= 32;
    }

    /* Finish without over-reading or over-writing the advertised length. */
    while (n--) {
        *out++ = *from++;
    }
    return out;
}

#endif /* SPARC_VIS1_CHUNKSET */
