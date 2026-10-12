/* inflate_fast_mmi.c -- inflate_fast() for the R5900.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Two interchangeable implementations of inflate_fast_mmi():
 *
 *  - default (MIPS_MMI_INFLATE_FAST): the generic fast-decode template
 *    instantiated with chunk primitives built on chunkmemset_safe_mmi_fast()
 *    (exact LZ77 copy semantics, QFSRV/LQ/SQ for long matches, overlapped
 *    stores for short ones). Only the fast variant; "safe" stays generic.
 *
 *  - MIPS_MMI_INFLATE_FAST_ASM: the whole symbol loop in hand-scheduled
 *    assembly (inflate_fast_asm.h), with the rare cases handled here in C.
 */
#ifdef MIPS_MMI

#include "zbuild.h"
#include "arch_functions.h"

#if defined(MIPS_MMI_INFLATE_FAST) && !defined(MIPS_MMI_INFLATE_FAST_ASM)

#define CHUNKSIZE  chunksize_mmi
static inline size_t chunksize_mmi(void) { return 8; }

/* The template may overwrite past out+len (up to 258 bytes of slack are
 * guaranteed); the exact copy below never does, which is always allowed. */
static inline uint8_t *chunkcopy_mmi(uint8_t *out, uint8_t const *from, size_t len) {
    return chunkmemset_safe_mmi_fast(out, (uint8_t *)from, len, len);
}
static inline uint8_t *chunkmemset_mmi(uint8_t *out, uint8_t *from, size_t len) {
    return chunkmemset_safe_mmi_fast(out, from, len, len);
}
/* Distances below the chunk size are handled inside the copy itself. */
static inline uint8_t *chunkunroll_mmi(uint8_t *out, unsigned *dist, unsigned *len) {
    (void)dist; (void)len;
    return out;
}
#define CHUNKCOPY    chunkcopy_mmi
#define CHUNKMEMSET  chunkmemset_mmi
#define CHUNKUNROLL  chunkunroll_mmi

static inline uint8_t *CHUNKCOPY_SAFE(uint8_t *out, uint8_t *from, size_t len, uint8_t *safe) {
    if (out == from)
        return out + len;
    const size_t safelen = (size_t)(safe - out);
    return chunkmemset_safe_mmi_fast(out, from, len, safelen);
}

#define INFLATE_FAST inflate_fast_mmi
#include "inffast_tpl.h"

#endif /* template instance */

#if defined(MIPS_MMI_INFLATE_FAST_ASM) && \
    !defined(INFLATE_STRICT) && !defined(INFLATE_ALLOW_INVALID_DISTANCE_TOOFAR_ARRR)

#include "zutil.h"
#include "inftrees.h"
#include "inflate.h"
#include "inflate_p.h"
#include "functable.h"

#include "inflate_fast_asm.h"

/* Exit codes of zng_inflate_burst():
 *   0 loop finished (input or output budget used up)
 *   1 end of block        2 invalid literal/length code
 *   3 invalid distance code
 *   4 a match reaching back before this call's output (len/dist in ctx). */
typedef struct {
    uint32_t in, last, out, end, beg, pad0;
    uint64_t hold;
    uint32_t bits, lcode, dcode, lmask, dmask, len, dist, pad1;
} burst_ctx;

void Z_INTERNAL inflate_fast_mmi(PREFIX3(stream) *strm, uint32_t start) {
    struct inflate_state *state = (struct inflate_state *)strm->state;
    burst_ctx c;
    unsigned char *beg, *safe, *window;
    unsigned wsize, whave, wnext;
    z_const unsigned char *last;

    c.in = (uint32_t)(uintptr_t)strm->next_in;
    last = strm->next_in + (strm->avail_in - (INFLATE_FAST_MIN_HAVE - 1));
    c.last = (uint32_t)(uintptr_t)last;
    c.out = (uint32_t)(uintptr_t)strm->next_out;
    beg = strm->next_out - (start - strm->avail_out);
    c.beg = (uint32_t)(uintptr_t)beg;
    safe = strm->next_out + strm->avail_out;
    c.end = (uint32_t)(uintptr_t)(safe - INFLATE_FAST_MIN_LEFT + 1);
    wsize = state->wsize;
    whave = state->whave;
    wnext = state->wnext;
    window = state->window;
    (void)wsize;
    c.hold = state->hold;
    c.bits = state->bits;
    c.lcode = (uint32_t)(uintptr_t)state->lencode;
    c.dcode = (uint32_t)(uintptr_t)state->distcode;
    c.lmask = (1U << state->lenbits) - 1;
    c.dmask = (1U << state->distbits) - 1;

    for (;;) {
        const int why = zng_inflate_burst(&c);
        if (why == 0)
            break;
        if (why == 1) {
            state->mode = TYPE;
            break;
        }
        if (why == 2) {
            SET_BAD("invalid literal/length code");
            break;
        }
        if (why == 3) {
            SET_BAD("invalid distance code");
            break;
        }
        /* why == 4: the match starts in the sliding window. */
        unsigned char *out = (unsigned char *)(uintptr_t)c.out;
        unsigned len = c.len, dist = c.dist;
        unsigned op = (unsigned)(out - beg);        /* max distance in output */
        op = dist - op;                              /* distance back in window */
        if (op > whave) {
            c.out = (uint32_t)(uintptr_t)out;
            SET_BAD("invalid distance too far back");
            break;
        }
        unsigned char *from = window;
        if (wnext == 0) {                            /* very common case */
            from += wsize - op;
        } else if (wnext >= op) {                    /* contiguous in window */
            from += wnext - op;
        } else {                                     /* wrap around window */
            op -= wnext;
            from += wsize - op;
            if (op < len) {                          /* some from end of window */
                len -= op;
                out = chunkmemset_safe_mmi_fast(out, from, op, (size_t)(safe - out));
                from = window;                       /* more from start of window */
                op = wnext;
            }
        }
        if (op < len) {                              /* still need some from output */
            len -= op;
            out = chunkmemset_safe_mmi_fast(out, from, op, (size_t)(safe - out));
            out = chunkmemset_safe_mmi_fast(out, out - dist, len, (size_t)(safe - out));
        } else {
            out = chunkmemset_safe_mmi_fast(out, from, len, (size_t)(safe - out));
        }
        c.out = (uint32_t)(uintptr_t)out;
        if (!(c.in < c.last && c.out < c.end))
            break;
    }

    unsigned char *in = (unsigned char *)(uintptr_t)c.in;
    unsigned char *out = (unsigned char *)(uintptr_t)c.out;
    uint64_t hold = c.hold;
    unsigned bits = c.bits;
    /* return unused bytes (on entry, bits < 8, so in won't go too far back) */
    unsigned unused = bits >> 3;
    in -= unused;
    bits -= unused << 3;
    hold &= (UINT64_C(1) << bits) - 1;

    strm->next_in = in;
    strm->next_out = out;
    strm->avail_in = (unsigned)(in < last ? (INFLATE_FAST_MIN_HAVE - 1) + (last - in)
                                          : (INFLATE_FAST_MIN_HAVE - 1) - (in - last));
    strm->avail_out = (unsigned)(safe - out);
    state->hold = (uint32_t)hold;
    state->bits = bits;
}

#endif /* asm instance */
#endif /* MIPS_MMI */
