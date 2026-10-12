/* chunkset_mmi_fast.c -- R5900 LZ77 history copy with unaligned head/tail.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * chunkmemset_safe_mmi_fast(out, from, len, left) has the exact semantics of
 * chunkmemset_safe_c: copy MIN(len, left) bytes from `from` to `out` as if
 * byte by byte (so a distance below the length repeats the pattern) and
 * return out + that count. Nothing past out + count is written.
 *
 * The earlier MMI routines peeled to a 16-byte boundary one byte at a time
 * (up to 15 iterations) and built the periodic pattern with a modulo per
 * byte; both cost more than the whole generic copy for the common short
 * lengths. This version has no byte loops:
 *   distance >= 16: the first and last 16 bytes are copied with unaligned
 *     LDL/LDR + SDL/SDR pairs; the body is quadword LQ/SQ with dst aligned.
 *     A source with a different residue is realigned with QFSRV (as in
 *     zng_mmi_memcpy). Loads are only issued ahead of stores when the
 *     distance guarantees that every byte that will be used was already
 *     written: 4 quads at once for >= 64 (co-aligned) / >= 80 (QFSRV),
 *     a carried quad for >= 32 (QFSRV), none otherwise.
 *   distance 1/2/4/8: the period divides 8, so one 64-bit pattern is built
 *     from the history (one load, a few shifts), stored unaligned at both
 *     ends, rotated to the destination phase and splatted with SQ.
 *   everything else: chunkmemset_safe_c.
 */
#ifdef MIPS_MMI

#include "zbuild.h"
#include "arch_functions.h"
#include "zmemory.h"

void *zng_mmi_memcpy(void *dst, const void *src, size_t n);

/* ---- assembly helpers (caller-saved registers only) -------------------- */
void zng_cm_fill(uint8_t *dst, size_t nq, const uint64_t *pattern);
void zng_cm_copy_a4(uint8_t *dst, const uint8_t *src, size_t nq4);
void zng_cm_copy_a1(uint8_t *dst, const uint8_t *src, size_t nq);
void zng_cm_copy_qf4(uint8_t *dst, const uint8_t *src, size_t nq4);
void zng_cm_copy_qf1c(uint8_t *dst, const uint8_t *src, size_t nq);
void zng_cm_copy_qf1n(uint8_t *dst, const uint8_t *src, size_t nq);

__asm__(
    ".text\n\t"
    ".align 4\n\t"
    ".set push\n\t.set noreorder\n\t.set noat\n"
    /* zng_cm_fill($4=dst 16-aligned, $5=nq, $6=pointer to the 64-bit pattern) */
    ".globl zng_cm_fill\n\t.hidden zng_cm_fill\n\t.type zng_cm_fill, @function\n"
    "zng_cm_fill:\n\t"
    "ld     $8, 0($6)\n\t"
    "pcpyld $8, $8, $8\n\t"
    "sltiu  $1, $5, 8\n\t"
    "bnez   $1, 2f\n\t"
    "nop\n"
    "1:\n\t"
    "sq     $8, 0($4)\n\t"
    "sq     $8, 16($4)\n\t"
    "sq     $8, 32($4)\n\t"
    "sq     $8, 48($4)\n\t"
    "sq     $8, 64($4)\n\t"
    "sq     $8, 80($4)\n\t"
    "sq     $8, 96($4)\n\t"
    "sq     $8, 112($4)\n\t"
    "addiu  $5, $5, -8\n\t"
    "sltiu  $1, $5, 8\n\t"
    "beqz   $1, 1b\n\t"
    "addiu  $4, $4, 128\n"
    "2:\n\t"
    "beqz   $5, 4f\n\t"
    "nop\n"
    "3:\n\t"
    "addiu  $5, $5, -1\n\t"
    "sq     $8, 0($4)\n\t"
    "bnez   $5, 3b\n\t"
    "addiu  $4, $4, 16\n"
    "4:\n\t"
    "jr     $31\n\t"
    "nop\n\t"
    ".size zng_cm_fill, .-zng_cm_fill\n"
    /* zng_cm_copy_a4($4=dst, $5=src, $6=nq4 >= 1): co-aligned, 4 loads then 4 stores */
    ".globl zng_cm_copy_a4\n\t.hidden zng_cm_copy_a4\n\t.type zng_cm_copy_a4, @function\n"
    "zng_cm_copy_a4:\n"
    "1:\n\t"
    "lq     $8, 0($5)\n\t"
    "lq     $9, 16($5)\n\t"
    "lq     $10, 32($5)\n\t"
    "lq     $11, 48($5)\n\t"
    "addiu  $6, $6, -1\n\t"
    "sq     $8, 0($4)\n\t"
    "sq     $9, 16($4)\n\t"
    "sq     $10, 32($4)\n\t"
    "sq     $11, 48($4)\n\t"
    "addiu  $5, $5, 64\n\t"
    "bnez   $6, 1b\n\t"
    "addiu  $4, $4, 64\n\t"
    "jr     $31\n\t"
    "nop\n\t"
    ".size zng_cm_copy_a4, .-zng_cm_copy_a4\n"
    /* zng_cm_copy_a1($4=dst, $5=src, $6=nq >= 0): one quad at a time */
    ".globl zng_cm_copy_a1\n\t.hidden zng_cm_copy_a1\n\t.type zng_cm_copy_a1, @function\n"
    "zng_cm_copy_a1:\n\t"
    "beqz   $6, 2f\n\t"
    "nop\n"
    "1:\n\t"
    "lq     $8, 0($5)\n\t"
    "addiu  $6, $6, -1\n\t"
    "addiu  $5, $5, 16\n\t"
    "sq     $8, 0($4)\n\t"
    "bnez   $6, 1b\n\t"
    "addiu  $4, $4, 16\n"
    "2:\n\t"
    "jr     $31\n\t"
    "nop\n\t"
    ".size zng_cm_copy_a1, .-zng_cm_copy_a1\n"
    /* zng_cm_copy_qf4($4=dst, $5=src unaligned, $6=nq4 >= 1): QFSRV, 4 quads ahead */
    ".globl zng_cm_copy_qf4\n\t.hidden zng_cm_copy_qf4\n\t.type zng_cm_copy_qf4, @function\n"
    "zng_cm_copy_qf4:\n\t"
    "mtsab  $5, 0\n\t"
    "lq     $8, 0($5)\n"
    "1:\n\t"
    "lq     $9, 16($5)\n\t"
    "lq     $10, 32($5)\n\t"
    "lq     $11, 48($5)\n\t"
    "lq     $12, 64($5)\n\t"
    "qfsrv  $13, $9, $8\n\t"
    "qfsrv  $14, $10, $9\n\t"
    "addiu  $6, $6, -1\n\t"
    "qfsrv  $15, $11, $10\n\t"
    "qfsrv  $24, $12, $11\n\t"
    "sq     $13, 0($4)\n\t"
    "sq     $14, 16($4)\n\t"
    "sq     $15, 32($4)\n\t"
    "sq     $24, 48($4)\n\t"
    "addiu  $5, $5, 64\n\t"
    "addiu  $4, $4, 64\n\t"
    "bnez   $6, 1b\n\t"
    "por    $8, $12, $0\n\t"
    "jr     $31\n\t"
    "nop\n\t"
    ".size zng_cm_copy_qf4, .-zng_cm_copy_qf4\n"
    /* zng_cm_copy_qf1c($4=dst, $5=src, $6=nq >= 0): one quad, carried block */
    ".globl zng_cm_copy_qf1c\n\t.hidden zng_cm_copy_qf1c\n\t.type zng_cm_copy_qf1c, @function\n"
    "zng_cm_copy_qf1c:\n\t"
    "beqz   $6, 2f\n\t"
    "nop\n\t"
    "mtsab  $5, 0\n\t"
    "lq     $8, 0($5)\n"
    "1:\n\t"
    "lq     $9, 16($5)\n\t"
    "addiu  $6, $6, -1\n\t"
    "addiu  $5, $5, 16\n\t"
    "qfsrv  $13, $9, $8\n\t"
    "addiu  $4, $4, 16\n\t"
    "sq     $13, -16($4)\n\t"
    "bnez   $6, 1b\n\t"
    "por    $8, $9, $0\n"
    "2:\n\t"
    "jr     $31\n\t"
    "nop\n\t"
    ".size zng_cm_copy_qf1c, .-zng_cm_copy_qf1c\n"
    /* zng_cm_copy_qf1n($4=dst, $5=src, $6=nq >= 0): no carry (distance 16..31) */
    ".globl zng_cm_copy_qf1n\n\t.hidden zng_cm_copy_qf1n\n\t.type zng_cm_copy_qf1n, @function\n"
    "zng_cm_copy_qf1n:\n\t"
    "beqz   $6, 2f\n\t"
    "nop\n\t"
    "mtsab  $5, 0\n"
    "1:\n\t"
    "lq     $8, 0($5)\n\t"
    "lq     $9, 16($5)\n\t"
    "addiu  $6, $6, -1\n\t"
    "addiu  $5, $5, 16\n\t"
    "qfsrv  $13, $9, $8\n\t"
    "addiu  $4, $4, 16\n\t"
    "bnez   $6, 1b\n\t"
    "sq     $13, -16($4)\n"
    "2:\n\t"
    "jr     $31\n\t"
    "nop\n\t"
    ".size zng_cm_copy_qf1n, .-zng_cm_copy_qf1n\n\t"
    ".set pop\n"
);

static inline void copy16_unaligned(uint8_t *dst, const uint8_t *src) {
    const uint64_t a = zng_memread_8(src);
    const uint64_t b = zng_memread_8(src + 8);
    zng_memwrite_8(dst, a);
    zng_memwrite_8(dst + 8, b);
}

/* Rotate the byte pattern so that byte j of the result is byte (j + s) mod 8. */
static inline uint64_t rotate_pattern(uint64_t v, unsigned s) {
    return s == 0 ? v : (v >> (8u * s)) | (v << (64u - 8u * s));
}

Z_INTERNAL uint8_t *chunkmemset_safe_mmi_fast(uint8_t *out, uint8_t *from,
                                              size_t len, size_t left) {
    const size_t n = MIN(len, left);
    if (n == 0)
        return out;
    const uintptr_t o = (uintptr_t)out, f = (uintptr_t)from;
    /* Disjoint source and destination (a window copy, or a distance of at
     * least the length): plain memcpy semantics. */
    if ((f > o ? f - o : o - f) >= n) {
        zng_mmi_memcpy(out, from, n);
        return out + n;
    }
    if (f >= o)
        return chunkmemset_safe_c(out, from, len, left);
    const size_t dist = (size_t)(o - f);
    uint8_t *const end = out + n;

    /* Short copies (the common LZ77 case): two overlapped stores of the
     * widest width w with dist >= w, so the second load only reads bytes the
     * first store (or the history) already holds: that needs n <= dist + w. */
    if (n < 32) {
        if (n >= 16 && dist >= 16) {
            copy16_unaligned(out, from);
            copy16_unaligned(end - 16, end - 16 - dist);
            return end;
        }
        if (n >= 8 && n < 16 && dist >= 8) {
            zng_memwrite_8(out, zng_memread_8(from));
            zng_memwrite_8(end - 8, zng_memread_8(end - 8 - dist));
            return end;
        }
        if (n >= 4 && n < 8 && dist >= 4) {
            zng_memwrite_4(out, zng_memread_4(from));
            zng_memwrite_4(end - 4, zng_memread_4(end - 4 - dist));
            return end;
        }
        if (n < 4 && dist >= 2) {
            if (n == 1) {
                *out = *from;
            } else {
                zng_memwrite_2(out, zng_memread_2(from));
                zng_memwrite_2(end - 2, zng_memread_2(end - 2 - dist));
            }
            return end;
        }
    }

    if (dist >= 16) {
        if (n < 32)
            return chunkmemset_safe_c(out, from, len, left);
        copy16_unaligned(out, from);
        uint8_t *dst = (uint8_t *)(((uintptr_t)out + 16u) & ~(uintptr_t)15u);
        const uint8_t *src = from + (dst - out);
        const size_t nq = (size_t)(end - dst) >> 4;
        if ((((uintptr_t)src) & 15u) == 0) {
            const size_t q4 = dist >= 64 ? nq >> 2 : 0;
            if (q4) {
                zng_cm_copy_a4(dst, src, q4);
                dst += q4 << 6;
                src += q4 << 6;
            }
            zng_cm_copy_a1(dst, src, nq - (q4 << 2));
        } else {
            size_t q4 = dist >= 80 ? nq >> 2 : 0;
            if (q4) {
                zng_cm_copy_qf4(dst, src, q4);
                dst += q4 << 6;
                src += q4 << 6;
            }
            if (dist >= 32)
                zng_cm_copy_qf1c(dst, src, nq - (q4 << 2));
            else
                zng_cm_copy_qf1n(dst, src, nq - (q4 << 2));
        }
        copy16_unaligned(end - 16, end - 16 - dist);
        return end;
    }

    if (dist == 1 || dist == 2 || dist == 4 || dist == 8) {
        uint64_t v;
        if (dist == 8) {
            v = zng_memread_8(from);
        } else if (dist == 4) {
            v = zng_memread_4(from);
            v |= v << 32;
        } else if (dist == 2) {
            v = zng_memread_2(from);
            v |= v << 16;
            v |= v << 32;
        } else {
            v = *from;
            v |= v << 8;
            v |= v << 16;
            v |= v << 32;
        }
        if (n < 16) {
            if (n < 8)
                return chunkmemset_safe_c(out, from, len, left);
            zng_memwrite_8(out, v);
            zng_memwrite_8(end - 8, rotate_pattern(v, (unsigned)(n & 7u)));
            return end;
        }
        zng_memwrite_8(out, v);
        zng_memwrite_8(out + 8, v);
        uint8_t *dst = (uint8_t *)(((uintptr_t)out + 15u) & ~(uintptr_t)15u);
        const size_t nq = (size_t)(end - dst) >> 4;
        if (nq)
        {
            const uint64_t phased = rotate_pattern(v, (unsigned)((dst - out) & 7u));
            zng_cm_fill(dst, nq, &phased);
        }
        const uint64_t tail = rotate_pattern(v, (unsigned)((n - 16u) & 7u));
        zng_memwrite_8(end - 16, tail);
        zng_memwrite_8(end - 8, tail);
        return end;
    }
    return chunkmemset_safe_c(out, from, len, left);
}

#endif /* MIPS_MMI */
