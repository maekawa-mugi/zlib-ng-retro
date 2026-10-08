/* Experimental Adler-32 for the PlayStation 2 Emotion Engine.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * The MMI expands 16 bytes to halfwords and computes eight pairwise sums.
 * The positional Adler sum remains exact scalar integer arithmetic; PMADDH
 * is deliberately avoided because it implicitly changes HI/LO registers.
 * Enable only via WITH_MMI_ADLER32, which defaults to OFF.
 */
#ifdef MIPS_MMI_ADLER32
#include "zbuild.h"
#include "arch_functions.h"
#include "adler32_p.h"

Z_INTERNAL uint32_t adler32_mmi(uint32_t adler, const uint8_t *buf, size_t len) {
    uint32_t a = adler & 0xffffu, b = adler >> 16;
    if (len < 16)
        return adler32_c(adler, buf, len);

    while (len != 0) {
        size_t n = MIN(len, (size_t)NMAX);
        len -= n;

        /* Keep each 32-bit sum in range; NMAX is the zlib Adler bound. */
        while (n != 0 && ((uintptr_t)buf & 15u) != 0) {
            a += *buf++;
            b += a;
            n--;
        }
        while (n >= 16) {
            uint16_t pairs[8] ALIGNED_(16);
            uint32_t sum16 = 0, weighted = 0;

            /* Both buf and pairs are 16-byte aligned (LQ/SQ mask low 4 bits).
             * On little-endian EE, lane i contains buf[i] + buf[8+i]. */
            __asm__ volatile (
                "lq     $8, 0(%[src])\n\t"
                "pextlb $9, $0, $8\n\t"
                "pextub $10, $0, $8\n\t"
                "paddh  $9, $9, $10\n\t"
                "sq     $9, 0(%[pairs])"
                :
                : [src] "r" (buf), [pairs] "r" (pairs)
                : "$8", "$9", "$10", "memory"
            );

            for (unsigned i = 0; i < 8; i++)
                sum16 += pairs[i];
            for (unsigned i = 0; i < 16; i++)
                weighted += (16u - i) * (uint32_t)buf[i];

            /* Equivalent to 16 iterations of b += a += byte. */
            b += 16u * a + weighted;
            a += sum16;
            buf += 16;
            n -= 16;
        }
        while (n-- != 0) {
            a += *buf++;
            b += a;
        }

        a %= BASE;
        b %= BASE;
    }
    return (b << 16) | a;
}

Z_INTERNAL uint32_t adler32_copy_mmi(uint32_t adler, uint8_t *dst,
                                     const uint8_t *src, size_t len) {
    uint32_t result = adler32_mmi(adler, src, len);
    memcpy(dst, src, len);
    return result;
}
#endif /* MIPS_MMI_ADLER32 */
