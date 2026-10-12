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

static inline void slide_hash_mmi_chain(Pos *table, uint32_t entries, Pos wsize,
                                        int interleaved) {
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

        /* Alternative schedule: load four independent quadwords before
         * subtracting, then store them. This may hide load/use latency,
         * but the additional live registers can also cost cycles on EE.
         * Keep the serial schedule for direct hardware A/B measurement. */
        if (interleaved == 1) {
            while (entries >= 32) {
                __asm__ volatile (
                    "lq     $8, 0(%[delta])\n\t"
                    "lq     $9, 0(%[ptr])\n\t"
                    "lq     $10, 16(%[ptr])\n\t"
                    "lq     $11, 32(%[ptr])\n\t"
                    "lq     $12, 48(%[ptr])\n\t"
                    "psubuh $9, $9, $8\n\t"
                    "psubuh $10, $10, $8\n\t"
                    "psubuh $11, $11, $8\n\t"
                    "psubuh $12, $12, $8\n\t"
                    "sq     $9, 0(%[ptr])\n\t"
                    "sq     $10, 16(%[ptr])\n\t"
                    "sq     $11, 32(%[ptr])\n\t"
                    "sq     $12, 48(%[ptr])"
                    :
                    : [delta] "r" (delta), [ptr] "r" (p)
                    : "$8", "$9", "$10", "$11", "$12", "memory"
                );
                p += 32;
                entries -= 32;
            }
        } else if (interleaved == 2) {
            /* Two-wide schedule: two independent loads between each load
             * and its first use, but only three live MMI GPRs. */
            while (entries >= 32) {
                __asm__ volatile (
                    "lq $8, 0(%[delta])\n\t"
                    "lq $9, 0(%[ptr])\n\t"
                    "lq $10, 16(%[ptr])\n\t"
                    "psubuh $9, $9, $8\n\t"
                    "psubuh $10, $10, $8\n\t"
                    "sq $9, 0(%[ptr])\n\t"
                    "sq $10, 16(%[ptr])\n\t"
                    "lq $9, 32(%[ptr])\n\t"
                    "lq $10, 48(%[ptr])\n\t"
                    "psubuh $9, $9, $8\n\t"
                    "psubuh $10, $10, $8\n\t"
                    "sq $9, 32(%[ptr])\n\t"
                    "sq $10, 48(%[ptr])"
                    : : [delta] "r" (delta), [ptr] "r" (p)
                    : "$8", "$9", "$10", "memory"
                );
                p += 32;
                entries -= 32;
            }
        } else {
            /* Baseline serial load/subtract/store schedule. */
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

/* Both versions are linked into EE builds so the test and benchmark can
 * compare identical inputs without requiring a second executable. */
void Z_INTERNAL slide_hash_mmi_serial(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size must fit in Pos");
    Pos wsize = (Pos)s->w_size;
    slide_hash_mmi_chain(s->head, HASH_SIZE, wsize, 0);
    slide_hash_mmi_chain(s->prev, (uint32_t)wsize, wsize, 0);
}

void Z_INTERNAL slide_hash_mmi_interleaved(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size must fit in Pos");
    Pos wsize = (Pos)s->w_size;
    slide_hash_mmi_chain(s->head, HASH_SIZE, wsize, 1);
    slide_hash_mmi_chain(s->prev, (uint32_t)wsize, wsize, 1);
}

void Z_INTERNAL slide_hash_mmi_interleaved2(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size must fit in Pos");
    slide_hash_mmi_chain(s->head, HASH_SIZE, (Pos)s->w_size, 2);
    slide_hash_mmi_chain(s->prev, s->w_size, (Pos)s->w_size, 2);
}

void Z_INTERNAL slide_hash_head_mmi_interleaved2(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size must fit in Pos");
    slide_hash_mmi_chain(s->head, HASH_SIZE, (Pos)s->w_size, 2);
}

void Z_INTERNAL slide_hash_head_mmi_serial(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size must fit in Pos");
    slide_hash_mmi_chain(s->head, HASH_SIZE, (Pos)s->w_size, 0);
}

void Z_INTERNAL slide_hash_head_mmi_interleaved(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size must fit in Pos");
    slide_hash_mmi_chain(s->head, HASH_SIZE, (Pos)s->w_size, 1);
}

void Z_INTERNAL slide_hash_mmi(deflate_state *s) {
#ifdef MIPS_MMI_SLIDE_HASH_INTERLEAVED2
    slide_hash_mmi_interleaved2(s);
#elif defined(MIPS_MMI_SLIDE_HASH_INTERLEAVED)
    slide_hash_mmi_interleaved(s);
#else
    slide_hash_mmi_serial(s);
#endif
}

void Z_INTERNAL slide_hash_head_mmi(deflate_state *s) {
#ifdef MIPS_MMI_SLIDE_HASH_INTERLEAVED2
    slide_hash_head_mmi_interleaved2(s);
#elif defined(MIPS_MMI_SLIDE_HASH_INTERLEAVED)
    slide_hash_head_mmi_interleaved(s);
#else
    slide_hash_head_mmi_serial(s);
#endif
}

#endif /* MIPS_MMI */
