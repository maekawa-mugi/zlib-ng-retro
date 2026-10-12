/* adler32_mmi_vec.h -- hand-scheduled R5900 MMI Adler-32 kernel.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Whole-window kernel written as a stand-alone assembly function so that
 * no C register allocation, prologue or scalar head/tail loops remain.
 *
 * Math. A window is nq (<=346) aligned quadwords. Misaligned heads and
 * short tails are handled by *virtual zero bytes*: the first quadword is
 * ANDed with a head mask and the last with a tail mask, and the resulting
 * excess terms are subtracted exactly at the end. Every quadword is split by
 * PEXTLB/PEXTUB against zero into two "units" of eight halfword lanes, and per
 * unit  P += unit; B += P  (all PADDH, no multiplier). After a group of
 * u units B[l] = sum_m (u-m)*unit_m[l]. A group is 11 quadwords (22 units)
 * so that B (<= 255*22*23/2 = 64515) and P never overflow 16 bits. Each
 * group is folded into 32-bit lane accumulators (BW, VPl/VPh, VR) with
 * PEXT{L,U}H + PADDW. With L = 176 bytes per full group, G groups, the
 * first group the partial one entered by computed jump:
 *     S1     = sum lanes(VP)
 *     S2part = L*(sum VR - S1) + 8*sum BW - sum_l l*VP[l]
 * and  b' = b + (N' - off)*a + S2part - pad*(a + S1),  a' = a + S1,  with
 * N' = 16*nq the virtual byte count, off the head padding and pad the tail
 * padding. Finally both sums are reduced mod 65521 with a multiply by the
 * magic 0x80078071 (exact for all 32-bit inputs).
 *
 * Register map (caller-saved only, so no stack frame):
 *   scalars: $4=a $5=b $6=group base $7=groups left $9=tail mask ptr
 *            $1=mask table base
 *   128-bit: $8=P $10=B $2=quadword $3=low half $12=high half
 *            $13=BW $14=VPl $15=VPh $24=VR $25=tail mask
 *
 * uint32_t adler32_mmi_vec_kernel(uint32_t adler, const uint8_t *p16,
 *     size_t nq, const uint8_t *head_mask, const uint8_t *tail_mask,
 *     size_t head_off, const uint8_t *tail_mask_base);
 * returns the reduced Adler word in v0.
 */
#ifndef ADLER32_MMI_VEC_H
#define ADLER32_MMI_VEC_H

/* One 16-byte block: 7 ops, 28 bytes. NEXT is the byte offset of the next
 * quadword relative to the notional group base ($6). */
#define ADLER_VEC_BLK(NEXT) \
    "pextlb $3, $0, $2\n\t" \
    "pextub $12, $0, $2\n\t" \
    "lq     $2, " #NEXT "($6)\n\t" \
    "paddh  $8, $8, $3\n\t" \
    "paddh  $10, $10, $8\n\t" \
    "paddh  $8, $8, $12\n\t" \
    "paddh  $10, $10, $8\n\t"

/* Last block of a group: applies the tail mask (all-ones except for the
 * very last group of the window). Entered by computed jump at block start. */
#define ADLER_VEC_BLK_LAST \
    "pand   $2, $2, $25\n\t" \
    "pextlb $3, $0, $2\n\t" \
    "pextub $12, $0, $2\n\t" \
    "paddh  $8, $8, $3\n\t" \
    "paddh  $10, $10, $8\n\t" \
    "paddh  $8, $8, $12\n\t" \
    "paddh  $10, $10, $8\n\t"

__asm__(
    ".text\n\t"
    ".align 4\n\t"
    ".globl adler32_mmi_vec_kernel\n\t"
    ".hidden adler32_mmi_vec_kernel\n\t"
    ".type  adler32_mmi_vec_kernel, @function\n\t"
    ".set push\n\t.set noreorder\n\t.set noat\n"
    "adler32_mmi_vec_kernel:\n\t"
    /* $4=adler $5=p $6=nq $7=head mask $8=tail mask $9=off $10=mask base */
    "lq     $12, 0($7)\n\t"              /* head mask */
    "move   $1, $10\n\t"                 /* mask table base */
    "move   $2, $5\n\t"                  /* p */
    "srl    $5, $4, 16\n\t"              /* b */
    "andi   $4, $4, 0xffff\n\t"          /* a */
    "move   $3, $6\n\t"                  /* nq */
    /* b += (16*nq - off) * a */
    "sll    $6, $3, 4\n\t"
    "subu   $6, $6, $9\n\t"
    "mult   $6, $6, $4\n\t"
    "addu   $5, $5, $6\n\t"
    /* q = nq/11 (exact for nq <= 346); rem = nq - 11q */
    "li     $6, 745\n\t"
    "mult   $6, $3, $6\n\t"
    "srl    $6, $6, 13\n\t"
    "sll    $10, $6, 3\n\t"
    "sll    $11, $6, 1\n\t"
    "addu   $10, $10, $11\n\t"
    "addu   $10, $10, $6\n\t"
    "subu   $10, $3, $10\n\t"            /* rem */
    "sltu   $11, $0, $10\n\t"
    "xori   $11, $11, 1\n\t"             /* rem == 0 */
    "subu   $7, $6, $11\n\t"             /* full groups after the first */
    "li     $11, 11\n\t"
    "subu   $11, $11, $10\n\t"
    "movz   $11, $0, $10\n\t"            /* skip = rem ? 11-rem : 0 */
    "sll    $10, $11, 5\n\t"
    "sll    $3, $11, 2\n\t"
    "subu   $10, $10, $3\n\t"            /* skip*28 */
    "la     $3, 4f\n\t"
    "addu   $10, $10, $3\n\t"            /* computed jump target */
    "sll    $3, $11, 4\n\t"
    "subu   $6, $2, $3\n\t"              /* group base = p - skip*16 */
    "move   $9, $8\n\t"                  /* tail mask ptr (kept for pad) */
    "lq     $2, 0($2)\n\t"               /* first quadword */
    "pxor   $8, $0, $0\n\t"
    "pxor   $13, $0, $0\n\t"
    "pand   $2, $2, $12\n\t"
    "pxor   $14, $0, $0\n\t"
    "pxor   $15, $0, $0\n\t"
    "pxor   $24, $0, $0\n\t"
    "move   $11, $1\n\t"
    "movz   $11, $9, $7\n\t"             /* last group already? use tail mask */
    "lq     $25, 0($11)\n\t"
    "jr     $10\n\t"
    "pxor   $10, $0, $0\n"               /* delay slot: B = 0 (target already read) */
    "4:\n\t"
    ADLER_VEC_BLK(16)  ADLER_VEC_BLK(32)  ADLER_VEC_BLK(48)
    ADLER_VEC_BLK(64)  ADLER_VEC_BLK(80)  ADLER_VEC_BLK(96)
    ADLER_VEC_BLK(112) ADLER_VEC_BLK(128) ADLER_VEC_BLK(144)
    ADLER_VEC_BLK(160) ADLER_VEC_BLK_LAST
    /* fold the group */
    "pextlh $3, $0, $10\n\t"
    "pextuh $12, $0, $10\n\t"
    "paddw  $13, $13, $3\n\t"
    "paddw  $13, $13, $12\n\t"
    "pextlh $3, $0, $8\n\t"
    "pextuh $12, $0, $8\n\t"
    "paddw  $14, $14, $3\n\t"
    "paddw  $15, $15, $12\n\t"
    "paddw  $24, $24, $14\n\t"
    "paddw  $24, $24, $15\n\t"
    "beqz   $7, 3f\n\t"
    "addiu  $6, $6, 176\n\t"
    "addiu  $7, $7, -1\n\t"
    "pxor   $8, $0, $0\n\t"
    "pxor   $10, $0, $0\n\t"
    "move   $11, $1\n\t"
    "movz   $11, $9, $7\n\t"
    "lq     $25, 0($11)\n\t"
    "b      4b\n\t"
    "lq     $2, 0($6)\n"
    "3:\n\t"
    /* horizontal reductions */
    "pcpyud $3, $13, $13\n\t"
    "paddw  $3, $3, $13\n\t"
    "dsrl32 $11, $3, 0\n\t"
    "addu   $3, $3, $11\n\t"             /* $3 = sum BW */
    "pcpyud $11, $24, $24\n\t"
    "paddw  $11, $11, $24\n\t"
    "dsrl32 $2, $11, 0\n\t"
    "addu   $11, $11, $2\n\t"            /* $11 = sum VR */
    "paddw  $12, $14, $15\n\t"           /* V = VPl + VPh */
    "pcpyud $2, $15, $15\n\t"
    "paddw  $2, $2, $15\n\t"
    "dsrl32 $25, $2, 0\n\t"
    "addu   $2, $2, $25\n\t"             /* $2 = hsum(VPh) */
    "dsrl32 $25, $12, 0\n\t"             /* w1 */
    "pcpyud $10, $12, $12\n\t"           /* $10 = (w2,w3) */
    "dsrl32 $8, $10, 0\n\t"              /* w3 */
    "addu   $6, $12, $25\n\t"
    "addu   $6, $6, $10\n\t"
    "addu   $6, $6, $8\n\t"              /* $6 = S1 = w0+w1+w2+w3 */
    "sll    $12, $10, 1\n\t"
    "addu   $12, $12, $25\n\t"           /* w1 + 2*w2 */
    "sll    $10, $8, 1\n\t"
    "addu   $10, $10, $8\n\t"            /* 3*w3 */
    "addu   $12, $12, $10\n\t"
    "sll    $2, $2, 2\n\t"
    "addu   $12, $12, $2\n\t"            /* weighted = sum l*VP[l] */
    "subu   $11, $11, $6\n\t"            /* sumVR - S1 */
    "li     $2, 176\n\t"
    "mult   $11, $11, $2\n\t"
    "sll    $3, $3, 3\n\t"
    "addu   $11, $11, $3\n\t"
    "subu   $11, $11, $12\n\t"
    "addu   $5, $5, $11\n\t"             /* b += S2part */
    "addu   $4, $4, $6\n\t"              /* a += S1 */
    "subu   $9, $9, $1\n\t"
    "srl    $9, $9, 4\n\t"               /* pad */
    "mult   $9, $9, $4\n\t"
    "subu   $5, $5, $9\n\t"              /* b -= pad * a */
    /* a %= 65521, b %= 65521 via mulhi(x, 0x80078071) >> 15 */
    "lui    $2, 0x8007\n\t"
    "ori    $2, $2, 0x8071\n\t"
    "li     $3, 65521\n\t"
    "multu  $4, $2\n\t"
    "mfhi   $6\n\t"
    "multu  $5, $2\n\t"
    "srl    $6, $6, 15\n\t"
    "mfhi   $7\n\t"
    "mult   $6, $6, $3\n\t"
    "srl    $7, $7, 15\n\t"
    "mult   $7, $7, $3\n\t"
    "subu   $4, $4, $6\n\t"
    "subu   $5, $5, $7\n\t"
    "sll    $2, $5, 16\n\t"
    "or     $2, $2, $4\n\t"
    "jr     $31\n\t"
    "nop\n\t"
    ".set pop\n\t"
    ".size adler32_mmi_vec_kernel, .-adler32_mmi_vec_kernel\n"
);

#endif /* ADLER32_MMI_VEC_H */
