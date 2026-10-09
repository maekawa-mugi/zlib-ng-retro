/* slide_hash_vis1.c -- SPARC VIS1 hash-chain window slide.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "arch_functions.h"

#ifdef SPARC_VIS1_SLIDEHASH

#include "deflate.h"

/* fcmpgt16 returns one bit for each 16-bit lane, with bit 3 corresponding
 * to the highest lane in memory (on big-endian SPARC). Precompute masks for
 * four 16-bit lanes to implement unsigned saturated subtraction by 0x8000.
 * Keep this 64-bit lookup naturally aligned for VIS ldd. */
static const uint64_t vis1_lane_masks[16] ALIGNED_(8) = {
    UINT64_C(0x0000000000000000),
    UINT64_C(0x000000000000ffff),
    UINT64_C(0x00000000ffff0000),
    UINT64_C(0x00000000ffffffff),
    UINT64_C(0x0000ffff00000000),
    UINT64_C(0x0000ffff0000ffff),
    UINT64_C(0x0000ffffffff0000),
    UINT64_C(0x0000ffffffffffff),
    UINT64_C(0xffff000000000000),
    UINT64_C(0xffff00000000ffff),
    UINT64_C(0xffff0000ffff0000),
    UINT64_C(0xffff0000ffffffff),
    UINT64_C(0xffffffff00000000),
    UINT64_C(0xffffffff0000ffff),
    UINT64_C(0xffffffffffff0000),
    UINT64_C(0xffffffffffffffff)
};

static inline void slide_hash_vis1_chain(Pos *table, uint32_t entries, Pos wsize) {
    Pos *p = table;

    /* The common DEFLATE window is 32768. For that value, m >= wsize
     * exactly when the signed 16-bit representation of m is negative.
     * For other window sizes retain the full generic semantics. */
    if (wsize != 32768) {
        for (uint32_t i = 0; i < entries; ++i) {
            Pos m = p[i];
            p[i] = (m >= wsize) ? (Pos)(m - wsize) : 0;
        }
        return;
    }

    /* SPARC ldd/std require 8-byte alignment. Peel up to 3 positions. */
    while (entries && ((uintptr_t)p & 7u)) {
        Pos m = *p;
        *p++ = (m >= wsize) ? (Pos)(m - wsize) : 0;
        --entries;
    }

    uint32_t blocks = entries / 4;
    if (blocks) {
        /* VIS lanes represent four successive Pos entries:
         *  1. fcmpgt16(0, input) gives the 4-bit high-bit mask.
         *  2. fpsub16(input, 0x8000) does wrapping subtraction.
         *  3. fand with the expanded lane mask zeros negative results.
         *
         * The fixed FP registers are fully clobbered, and the VIS-only
         * source file is compiled separately from the baseline code. */
        const uint64_t delta ALIGNED_(8) = UINT64_C(0x8000800080008000);
        const uint64_t *mask_table = vis1_lane_masks;
        uintptr_t mask_addr;
        Pos *const end = p + blocks * 4;

        __asm__ __volatile__(
            "ldd [%3], %%f2\n\t"
            "fzero %%f8\n\t"
            "1:\n\t"
            "ldd [%0], %%f0\n\t"
            "fcmpgt16 %%f8, %%f0, %2\n\t"
            "fpsub16 %%f0, %%f2, %%f4\n\t"
            "sll %2, 3, %2\n\t"
            "add %4, %2, %2\n\t"
            "ldd [%2], %%f6\n\t"
            "fand %%f4, %%f6, %%f4\n\t"
            "std %%f4, [%0]\n\t"
            "add %0, 8, %0\n\t"
            "subcc %1, 1, %1\n\t"
            "bne 1b\n\t"
            "nop"
            : "+&r" (p), "+&r" (blocks), "=&r" (mask_addr)
            : "r" (&delta), "r" (mask_table)
            : "cc", "memory", "f0", "f1", "f2", "f3",
              "f4", "f5", "f6", "f7", "f8", "f9");

        /* Ensure p is the correctly advanced end even if the assembler
         * reuses the counter or pointer operands. */
        p = end;
    }
    entries %= 4;
    while (entries--) {
        Pos m = *p;
        *p++ = (m >= wsize) ? (Pos)(m - wsize) : 0;
    }
}

Z_INTERNAL void slide_hash_vis1(deflate_state *s) {
    Assert(sizeof(Pos) == 2, "VIS1 slide_hash requires 16-bit Pos");
    Assert(s->w_size <= UINT16_MAX, "w_size must fit in Pos");
    Pos wsize = (Pos)s->w_size;

    slide_hash_vis1_chain(s->head, HASH_SIZE, wsize);
    slide_hash_vis1_chain(s->prev, wsize, wsize);
}

Z_INTERNAL void slide_hash_head_vis1(deflate_state *s) {
    Assert(sizeof(Pos) == 2, "VIS1 slide_hash requires 16-bit Pos");
    Assert(s->w_size <= UINT16_MAX, "w_size must fit in Pos");
    slide_hash_vis1_chain(s->head, HASH_SIZE, (Pos)s->w_size);
}

#endif /* SPARC_VIS1_SLIDEHASH */
