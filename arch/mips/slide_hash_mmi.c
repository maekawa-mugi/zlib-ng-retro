/* slide_hash_mmi.c -- R5900 MMI implementation of deflate hash table sliding
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * The R5900 PSUBUH instruction performs eight independent unsigned saturating
 * 16-bit subtractions. This is exactly the operation used by slide_hash_c.
 *
 * MMI LQ/SQ silently round addresses down to 16 bytes. Handle unaligned
 * prefixes and trailing entries in C, and issue quadword accesses only at
 * aligned addresses. This file is compiled only for an explicitly selected
 * PS2 EE target, never for ordinary MIPS/MSA targets.
 */
#ifdef MIPS_MMI

#include "zbuild.h"
#include "deflate.h"

_Static_assert(sizeof(Pos) == sizeof(uint16_t), "MMI slide_hash requires 16-bit Pos");

static inline void slide_hash_mmi_chain(Pos *table, uint32_t entries, Pos wsize) {
    Pos *p = table;

    /* Neither LQ nor SQ preserves the low four address bits. */
    while (entries != 0 && (((uintptr_t)p & 15u) != 0)) {
        Pos value = *p;
        *p++ = value >= wsize ? value - wsize : 0;
        entries--;
    }

    if (entries >= 8) {
        /* PSUBUH subtracts eight unsigned halfwords with saturation to zero. */
        uint16_t delta[8] ALIGNED_(16);
        for (unsigned i = 0; i < 8; i++)
            delta[i] = wsize;

        /* Process 64 bytes per iteration to amortize the delta load. */
        while (entries >= 32) {
            __asm__ volatile (
                "lq     $8, 0(%[delta])\n\t"
                "lq     $9, 0(%[ptr])\n\t"
                "psubuh $9, $9, $8\n\t"
                "sq     $9, 0(%[ptr])\n\t"
                "lq     $9, 16(%[ptr])\n\t"
                "psubuh $9, $9, $8\n\t"
                "sq     $9, 16(%[ptr])\n\t"
                "lq     $9, 32(%[ptr])\n\t"
                "psubuh $9, $9, $8\n\t"
                "sq     $9, 32(%[ptr])\n\t"
                "lq     $9, 48(%[ptr])\n\t"
                "psubuh $9, $9, $8\n\t"
                "sq     $9, 48(%[ptr])"
                :
                : [delta] "r" (delta), [ptr] "r" (p)
                : "$8", "$9", "memory"
            );
            p += 32;
            entries -= 32;
        }

        while (entries >= 8) {
            __asm__ volatile (
                "lq     $8, 0(%[delta])\n\t"
                "lq     $9, 0(%[ptr])\n\t"
                "psubuh $9, $9, $8\n\t"
                "sq     $9, 0(%[ptr])"
                :
                : [delta] "r" (delta), [ptr] "r" (p)
                : "$8", "$9", "memory"
            );
            p += 8;
            entries -= 8;
        }
    }

    while (entries-- != 0) {
        Pos value = *p;
        *p++ = value >= wsize ? value - wsize : 0;
    }
}

void Z_INTERNAL slide_hash_mmi(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size must fit in Pos");
    Pos wsize = (Pos)s->w_size;
    slide_hash_mmi_chain(s->head, HASH_SIZE, wsize);
    slide_hash_mmi_chain(s->prev, (uint32_t)wsize, wsize);
}

void Z_INTERNAL slide_hash_head_mmi(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size must fit in Pos");
    slide_hash_mmi_chain(s->head, HASH_SIZE, (Pos)s->w_size);
}

#endif /* MIPS_MMI */
