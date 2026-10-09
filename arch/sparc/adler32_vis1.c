/* adler32_vis1.c -- Adler-32 using SPARC VIS1 4x16-bit accumulation.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "arch_functions.h"

#ifdef SPARC_VIS1

#include "adler32_p.h"

/* Compute lane totals and running lane-prefix sums for exactly 64 bytes.
 * VIS1 fpmerge interleaves 32-bit input bytes with zero bytes, giving
 * exact unsigned 16-bit lane values (without the x16 scaling of fexpand).
 * The four lanes correspond to successive input byte offsets 0..3.
 *
 * There are 16 groups of four bytes. A lane can accumulate up to 4080
 * and a prefix lane to 255*(16*17/2) = 34680, both safely below 65536.
 * The 64-byte input need only be 4-byte aligned (ld), and the two
 * 64-bit output buffers must be aligned to eight bytes (std).
 * The updated loop operands require early-clobber constraints because
 * the output pointers are still read after loop-pointer modifications. */
static inline void vis1_accumulate64(const uint8_t *buf, uint16_t *lane_sum,
                                     uint16_t *lane_prefix) {
    const uint8_t *p = buf;
    unsigned groups = 16;

    __asm__ __volatile__(
        "fzero %%f4\n\t"
        "fzero %%f6\n\t"
        "fzero %%f8\n\t"
        "1:\n\t"
        "ld [%0], %%f0\n\t"
        "fpmerge %%f8, %%f0, %%f2\n\t"
        "fpadd16 %%f4, %%f2, %%f4\n\t"
        "fpadd16 %%f6, %%f4, %%f6\n\t"
        "add %0, 4, %0\n\t"
        "subcc %1, 1, %1\n\t"
        "bne 1b\n\t"
        "nop\n\t"
        "std %%f4, [%2]\n\t"
        "std %%f6, [%3]"
        : "+&r" (p), "+&r" (groups)
        : "r" (lane_sum), "r" (lane_prefix)
        : "cc", "memory", "f0", "f1", "f2", "f3",
          "f4", "f5", "f6", "f7", "f8", "f9");
}

Z_INTERNAL uint32_t adler32_vis1(uint32_t adler, const uint8_t *buf, size_t len) {
    uint32_t a = adler & 0xffffu;
    uint32_t b = adler >> 16;

    /* Small inputs are better handled by the existing scalar algorithm.
     * Its fallback stays compiled, even for VIS1-only builds. */
    if (len < 64)
        return adler32_c(adler, buf, len);

    /* Rebase at most every NMAX bytes, just like the generic implementation.
     * The unsigned 32-bit b accumulator therefore cannot overflow. */
    while (len) {
        size_t chunk = MIN(len, (size_t)NMAX);
        size_t left = chunk;

        /* VIS1 ld requires four-byte alignment; peel bytes first. */
        while (left && ((uintptr_t)buf & 3u)) {
            a += *buf++;
            b += a;
            --left;
        }

        while (left >= 64) {
            uint16_t sums[8] ALIGNED_(8);
            vis1_accumulate64(buf, sums, sums + 4);

            uint32_t total = (uint32_t)sums[0] + sums[1] + sums[2] + sums[3];
            uint32_t prefixes = (uint32_t)sums[4] + sums[5] + sums[6] + sums[7];
            uint32_t offsets = (uint32_t)sums[1] + 2u * sums[2] + 3u * sums[3];

            /* For 16 groups of 4 bytes, the weight of byte (i,j) in
             * Adler's second sum is 4*(16-i)-j, i=0..15, j=0..3.
             * Fold the 16-bit prefix sums into the exact scalar sum. */
            b += 64u * a + (4u * prefixes - offsets);
            a += total;
            buf += 64;
            left -= 64;
        }

        while (left) {
            a += *buf++;
            b += a;
            --left;
        }

        a %= BASE;
        b %= BASE;
        len -= chunk;
    }

    return a | (b << 16);
}

Z_INTERNAL uint32_t adler32_copy_vis1(uint32_t adler, uint8_t *dst,
                                      const uint8_t *src, size_t len) {
    adler = adler32_vis1(adler, src, len);
    memcpy(dst, src, len);
    return adler;
}

#endif /* SPARC_VIS1 */
