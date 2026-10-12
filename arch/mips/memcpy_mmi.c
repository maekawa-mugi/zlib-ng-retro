/* memcpy_mmi.c -- R5900 hand-written non-overlapping copy engine.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * void *zng_mmi_memcpy(void *dst, const void *src, size_t n);
 *
 * The libc memcpy of the PS2 SDK runs at ~9 cycles/byte when source and
 * destination alignments differ. This routine keeps every wide store
 * 16-byte aligned and realigns the source with QFSRV:
 *   - n < 32: overlapped first/last unaligned loads and stores
 *     (LDL/LDR + SDL/SDR for 8..31, LWL/LWR for 4..7, bytes for 1..3);
 *   - n >= 32: the first and last 16 bytes are loaded up front and stored
 *     unaligned (they also cover the unaligned head and the ragged tail);
 *     the body is a quadword loop with dst aligned:
 *       co-aligned source : LQ/SQ, four quadwords per iteration;
 *       misaligned source : LQ of the aligned neighbours + QFSRV (the
 *       funnel shift amount comes from MTSAB on the source address).
 * LQ never reads beyond the aligned quadword that holds a byte of the
 * source range, so no page boundary can be crossed.
 * Only caller-saved registers are used; there is no stack frame.
 */
#ifdef MIPS_MMI
#include "zbuild.h"

void *zng_mmi_memcpy(void *dst, const void *src, size_t n);

__asm__(
    ".text\n\t"
    ".align 4\n\t"
    ".globl zng_mmi_memcpy\n\t"
    ".type  zng_mmi_memcpy, @function\n\t"
    ".set push\n\t.set noreorder\n\t.set noat\n"
    "zng_mmi_memcpy:\n\t"
    "sltiu  $1, $6, 32\n\t"
    "bnez   $1, 20f\n\t"
    "move   $2, $4\n\t"                      /* delay: return value */
    /* ---- n >= 32 ---- */
    "ldr    $8, 0($5)\n\t"
    "ldl    $8, 7($5)\n\t"
    "ldr    $9, 8($5)\n\t"
    "ldl    $9, 15($5)\n\t"
    "addu   $11, $5, $6\n\t"
    "ldr    $3, -16($11)\n\t"
    "ldl    $3, -9($11)\n\t"
    "ldr    $7, -8($11)\n\t"
    "ldl    $7, -1($11)\n\t"
    "addu   $15, $4, $6\n\t"                 /* dst end */
    "sdr    $8, 0($4)\n\t"
    "sdl    $8, 7($4)\n\t"
    "sdr    $9, 8($4)\n\t"
    "sdl    $9, 15($4)\n\t"
    "andi   $13, $4, 15\n\t"
    "li     $14, 16\n\t"
    "subu   $13, $14, $13\n\t"               /* head = 16 - (dst & 15) */
    "addu   $4, $4, $13\n\t"                 /* dst now 16-byte aligned */
    "addu   $5, $5, $13\n\t"
    "subu   $6, $6, $13\n\t"
    "srl    $6, $6, 4\n\t"                   /* quadwords, >= 1 */
    "andi   $14, $5, 15\n\t"
    "beqz   $14, 10f\n\t"
    "nop\n\t"
    /* ---- misaligned source: LQ + QFSRV ---- */
    "mtsab  $5, 0\n\t"
    "lq     $8, 0($5)\n\t"                   /* aligned quad holding src[0] */
    "sltiu  $1, $6, 4\n\t"
    "bnez   $1, 6f\n\t"
    "nop\n"
    "5:\n\t"                                 /* four quadwords per iteration */
    "lq     $9, 16($5)\n\t"
    "lq     $10, 32($5)\n\t"
    "lq     $11, 48($5)\n\t"
    "lq     $12, 64($5)\n\t"
    "qfsrv  $13, $9, $8\n\t"
    "qfsrv  $14, $10, $9\n\t"
    "addiu  $6, $6, -4\n\t"
    "qfsrv  $24, $11, $10\n\t"
    "qfsrv  $25, $12, $11\n\t"
    "sq     $13, 0($4)\n\t"
    "sq     $14, 16($4)\n\t"
    "sq     $24, 32($4)\n\t"
    "sq     $25, 48($4)\n\t"
    "addiu  $5, $5, 64\n\t"
    "addiu  $4, $4, 64\n\t"
    "sltiu  $1, $6, 4\n\t"
    "beqz   $1, 5b\n\t"
    "por    $8, $12, $0\n"                   /* delay: carry last quad */
    "6:\n\t"
    "beqz   $6, 8f\n\t"
    "nop\n"
    "7:\n\t"                                 /* single quadword */
    "lq     $9, 16($5)\n\t"
    "addiu  $6, $6, -1\n\t"
    "addiu  $5, $5, 16\n\t"
    "qfsrv  $13, $9, $8\n\t"
    "addiu  $4, $4, 16\n\t"
    "sq     $13, -16($4)\n\t"
    "bnez   $6, 7b\n\t"
    "por    $8, $9, $0\n"
    "8:\n\t"
    "sdr    $3, -16($15)\n\t"                /* ragged tail (loaded up front) */
    "sdl    $3, -9($15)\n\t"
    "sdr    $7, -8($15)\n\t"
    "jr     $31\n\t"
    "sdl    $7, -1($15)\n"
    /* ---- co-aligned source: LQ/SQ ---- */
    "10:\n\t"
    "sltiu  $1, $6, 4\n\t"
    "bnez   $1, 12f\n\t"
    "nop\n"
    "11:\n\t"
    "lq     $8, 0($5)\n\t"
    "lq     $9, 16($5)\n\t"
    "lq     $13, 32($5)\n\t"
    "lq     $14, 48($5)\n\t"
    "addiu  $6, $6, -4\n\t"
    "sq     $8, 0($4)\n\t"
    "sq     $9, 16($4)\n\t"
    "sq     $13, 32($4)\n\t"
    "sq     $14, 48($4)\n\t"
    "addiu  $5, $5, 64\n\t"
    "sltiu  $1, $6, 4\n\t"
    "beqz   $1, 11b\n\t"
    "addiu  $4, $4, 64\n"
    "12:\n\t"
    "beqz   $6, 8b\n\t"
    "nop\n"
    "13:\n\t"
    "lq     $8, 0($5)\n\t"
    "addiu  $6, $6, -1\n\t"
    "addiu  $5, $5, 16\n\t"
    "sq     $8, 0($4)\n\t"
    "bnez   $6, 13b\n\t"
    "addiu  $4, $4, 16\n\t"
    "b      8b\n\t"
    "nop\n"
    /* ---- n < 32 ---- */
    "20:\n\t"
    "sltiu  $1, $6, 16\n\t"
    "beqz   $1, 24f\n\t"
    "sltiu  $1, $6, 8\n\t"
    "beqz   $1, 23f\n\t"
    "sltiu  $1, $6, 4\n\t"
    "beqz   $1, 22f\n\t"
    "addu   $9, $5, $6\n\t"
    "beqz   $6, 21f\n\t"
    "srl    $11, $6, 1\n\t"
    "lbu    $8, 0($5)\n\t"                   /* 1..3 bytes */
    "lbu    $10, -1($9)\n\t"
    "addu   $11, $5, $11\n\t"
    "lbu    $12, 0($11)\n\t"
    "srl    $14, $6, 1\n\t"
    "addu   $13, $4, $6\n\t"
    "addu   $14, $4, $14\n\t"
    "sb     $8, 0($4)\n\t"
    "sb     $12, 0($14)\n\t"
    "sb     $10, -1($13)\n"
    "21:\n\t"
    "jr     $31\n\t"
    "nop\n"
    "22:\n\t"                                /* 4..7 */
    "lwr    $8, 0($5)\n\t"
    "lwl    $8, 3($5)\n\t"
    "lwr    $10, -4($9)\n\t"
    "lwl    $10, -1($9)\n\t"
    "addu   $13, $4, $6\n\t"
    "swr    $8, 0($4)\n\t"
    "swl    $8, 3($4)\n\t"
    "swr    $10, -4($13)\n\t"
    "jr     $31\n\t"
    "swl    $10, -1($13)\n"
    "23:\n\t"                                /* 8..15 */
    "addu   $9, $5, $6\n\t"
    "ldr    $8, 0($5)\n\t"
    "ldl    $8, 7($5)\n\t"
    "ldr    $10, -8($9)\n\t"
    "ldl    $10, -1($9)\n\t"
    "addu   $13, $4, $6\n\t"
    "sdr    $8, 0($4)\n\t"
    "sdl    $8, 7($4)\n\t"
    "sdr    $10, -8($13)\n\t"
    "jr     $31\n\t"
    "sdl    $10, -1($13)\n"
    "24:\n\t"                                /* 16..31 */
    "addu   $9, $5, $6\n\t"
    "ldr    $8, 0($5)\n\t"
    "ldl    $8, 7($5)\n\t"
    "ldr    $10, 8($5)\n\t"
    "ldl    $10, 15($5)\n\t"
    "ldr    $11, -16($9)\n\t"
    "ldl    $11, -9($9)\n\t"
    "ldr    $12, -8($9)\n\t"
    "ldl    $12, -1($9)\n\t"
    "addu   $13, $4, $6\n\t"
    "sdr    $8, 0($4)\n\t"
    "sdl    $8, 7($4)\n\t"
    "sdr    $10, 8($4)\n\t"
    "sdl    $10, 15($4)\n\t"
    "sdr    $11, -16($13)\n\t"
    "sdl    $11, -9($13)\n\t"
    "sdr    $12, -8($13)\n\t"
    "jr     $31\n\t"
    "sdl    $12, -1($13)\n\t"
    ".set pop\n\t"
    ".size zng_mmi_memcpy, .-zng_mmi_memcpy\n"
);
#endif /* MIPS_MMI */
