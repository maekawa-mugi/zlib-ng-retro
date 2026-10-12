/* crc32_gather_mmi.c -- R5900 MMI zero-polynomial CRC-32, "gather" form.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Same mathematics as crc32_chorba_mmi.c (Sam Russell, "Chorba",
 * arXiv:2412.16398, with each polynomial raised to the 128th power so every
 * tap sits on a 16-byte boundary), evaluated differently. The scatter form
 * updates ten future ring slots per block with load/xor/store each; the
 * gather form keeps the transformed stream in a ring and computes
 *     v[i] = in[i] ^ v[i-t1] ^ v[i-t2] ^ ... ^ v[i-tT]
 * with one load per tap, T xors and a single store, in hand-written assembly
 * (crc32_gather_mmi.h). The look-ahead the stream still owes after the last
 * block is rebuilt from the same ring, checksummed with the braid code and
 * mapped back through the inverse of x^(16*degree*8) (precomputed matrix).
 *
 * Two polynomials cover the useful range (both divisible by the reflected
 * CRC-32 generator, verified offline):
 *   chorba352  degree  44, taps 10  -- medium inputs
 *   small300   degree 300, taps  4  -- large inputs
 * Everything lives on the caller's stack (<= 10 KiB): the code is reentrant.
 */
#ifdef MIPS_MMI_CHORBA

#include "zbuild.h"
#include "arch_functions.h"

#include "crc32_gather_mmi.h"

/* Hand-scheduled 4-braid table CRC for short inputs and for the look-ahead
 * residue (see crc32_braid4_mmi.h). Same results as crc32_braid(). */
#include "crc32_braid_tbl.h"
#include "crc32_braid4_mmi.h"

static inline uint32_t braid4_word(uint32_t d) {
    d = (d >> 8) ^ crc_table[d & 0xff];
    d = (d >> 8) ^ crc_table[d & 0xff];
    d = (d >> 8) ^ crc_table[d & 0xff];
    return (d >> 8) ^ crc_table[d & 0xff];
}

Z_INTERNAL uint32_t crc32_braid_mmi_asm(uint32_t crc, const uint8_t *buf, size_t len) {
    crc = ~crc;
    if (len >= 16u + 3u) {
        size_t head = (size_t)(-(uintptr_t)buf & 3u);
        for (; head; --head, --len)
            crc = (crc >> 8) ^ crc_table[(crc ^ *buf++) & 0xff];
        size_t blks = len >> 4;
        len &= 15u;
        const uint32_t *words = (const uint32_t *)buf;
        uint32_t st[4] ALIGNED_(16) = { crc, 0u, 0u, 0u };
        if (blks > 1u) {
            zng_braid4_loop(st, words, blks - 1u, zng_braid4_tbl);
            words += (blks - 1u) << 2;
        }
        uint32_t comb = braid4_word(st[0] ^ words[0]);
        comb = braid4_word(st[1] ^ words[1] ^ comb);
        comb = braid4_word(st[2] ^ words[2] ^ comb);
        comb = braid4_word(st[3] ^ words[3] ^ comb);
        crc = comb;
        buf = (const uint8_t *)(words + 4);
    }
    while (len--)
        crc = (crc >> 8) ^ crc_table[(crc ^ *buf++) & 0xff];
    return ~crc;
}

#ifndef MIPS_MMI_GATHER_THRESHOLD
#  define MIPS_MMI_GATHER_THRESHOLD 3072u
#endif
#ifndef MIPS_MMI_GATHER_LARGE
#  define MIPS_MMI_GATHER_LARGE 20480u
#endif
#ifndef MIPS_MMI_GATHER_MID
#  define MIPS_MMI_GATHER_MID 6144u
#endif

#define GATHER_RING_BYTES 8192u
#define GATHER_TMP_BLOCKS 300u /* >= degree of every polynomial here */

typedef void (*gather_kernel)(uint8_t *, const uint8_t *, size_t, const uint32_t *);

typedef struct {
    uint32_t degree, taps_n, minimum;
    uint32_t taps[10];         /* byte offsets, ascending */
    uint32_t unshift[32];
    gather_kernel kernel;
} gather_poly;

static const gather_poly gather_chorba352 = {
    44u, 10u, 1424u,
    { 16u, 48u, 112u, 144u, 192u, 208u, 448u, 592u, 624u, 704u },
    {
        0x65ae920eu, 0xcb5d241cu, 0x4dcb4e79u, 0x9b969cf2u,
        0xec5c3fa5u, 0x03c9790bu, 0x0792f216u, 0x0f25e42cu,
        0x1e4bc858u, 0x3c9790b0u, 0x792f2160u, 0xf25e42c0u,
        0x3fcd83c1u, 0x7f9b0782u, 0xff360f04u, 0x251d1849u,
        0x4a3a3092u, 0x94746124u, 0xf399c409u, 0x3c428e53u,
        0x78851ca6u, 0xf10a394cu, 0x396574d9u, 0x72cae9b2u,
        0xe595d364u, 0x105aa089u, 0x20b54112u, 0x416a8224u,
        0x82d50448u, 0xdedb0ed1u, 0x66c71be3u, 0xcd8e37c6u
    },
    zng_crcg_k10
};

static const gather_poly gather_t8_66 = {
    66u, 8u, 1040u,
    { 144u, 464u, 544u, 752u, 768u, 1008u, 1024u, 1056u },
    {
        0x00d533dau, 0x01aa67b4u, 0x0354cf68u, 0x06a99ed0u,
        0x0d533da0u, 0x1aa67b40u, 0x354cf680u, 0x6a99ed00u,
        0xd533da00u, 0x7116b241u, 0xe22d6482u, 0x1f2bcf45u,
        0x3e579e8au, 0x7caf3d14u, 0xf95e7a28u, 0x29cdf211u,
        0x539be422u, 0xa737c844u, 0x951e96c9u, 0xf14c2bd3u,
        0x39e951e7u, 0x73d2a3ceu, 0xe7a5479cu, 0x143b8979u,
        0x287712f2u, 0x50ee25e4u, 0xa1dc4bc8u, 0x98c991d1u,
        0xeae225e3u, 0x0eb54d87u, 0x1d6a9b0eu, 0x3ad5361cu
    },
    zng_crcg_k8
};

static const gather_poly gather_small300 = {
    300u, 4u, 9616u,
    { 2320u, 2928u, 3376u, 4800u },
    {
        0x3d8243d3u, 0x7b0487a6u, 0xf6090f4cu, 0x376318d9u,
        0x6ec631b2u, 0xdd8c6364u, 0x6069c089u, 0xc0d38112u,
        0x5ad60465u, 0xb5ac08cau, 0xb02917d5u, 0xbb2329ebu,
        0xad375597u, 0x811fad6fu, 0xd94e5c9fu, 0x69edbf7fu,
        0xd3db7efeu, 0x7cc7fbbdu, 0xf98ff77au, 0x286ee8b5u,
        0x50ddd16au, 0xa1bba2d4u, 0x980643e9u, 0xeb7d8193u,
        0x0d8a0567u, 0x1b140aceu, 0x3628159cu, 0x6c502b38u,
        0xd8a05670u, 0x6a31aaa1u, 0xd4635542u, 0x73b7acc5u
    },
    zng_crcg_k4
};

/* Read-only zero source for the look-ahead rebuild. */
static const uint8_t gather_zero[GATHER_TMP_BLOCKS * 16u] ALIGNED_(16) = {0};

static uint32_t gather_unshift(const gather_poly *p, uint32_t value) {
    uint32_t result = 0;
    for (unsigned bit = 0; bit < 32; bit++)
        if (value & (1u << bit))
            result ^= p->unshift[bit];
    return result;
}

/* buf is 16-byte aligned and len >= p->minimum. */
static uint32_t gather_run(const gather_poly *p, uint32_t crc,
                           const uint8_t *buf, size_t len,
                           uint8_t *ring, size_t R, uint8_t *tmp, int depth) {
    uint8_t first[16] ALIGNED_(16);
    uint32_t rp[10] ALIGNED_(16);
    const size_t mask = R - 1u;
    const size_t nb = len >> 4, processed = nb << 4;
    const unsigned T = p->taps_n;
    const gather_kernel kernel = p->kernel;

    /* The stream is written starting P0 = max-tap bytes into the ring, so no
     * read pointer wraps at the start; [0, P0) is the zero history. */
    const size_t P0 = p->taps[T - 1];
    memset(ring, 0, P0);
    memcpy(first, buf, 16);
    /* Fold the seed into the first four bytes (reflected CRC, >= 4 bytes). */
    const uint32_t seed = ~crc;
    for (unsigned j = 0; j < 4; ++j)
        first[j] ^= (uint8_t)(seed >> (j * 8u));

    for (size_t i = 0; i < nb;) {
        const size_t pos = (P0 + (i << 4)) & mask;
        size_t run = i == 0 ? 1u : nb - i;
        size_t cap = (R - pos) >> 4;
        if (run > cap) run = cap;
        for (unsigned k = 0; k < T; ++k) {
            const size_t off = (P0 + (i << 4) + R - p->taps[k]) & mask;
            cap = (R - off) >> 4;
            if (run > cap) run = cap;
            rp[k] = (uint32_t)(uintptr_t)(ring + off);
        }
        kernel(ring + pos, i == 0 ? first : buf + (i << 4), run, rp);
        i += run;
    }

    /* Rebuild the look-ahead slots of blocks nb .. nb+degree-1 into tmp:
     * slot r sums the stream values still owed, i.e. the taps with
     * 16*r < t_k (later taps read the zero block). One braid call then
     * covers the whole look-ahead. */
    for (size_t r = 0; r < p->degree;) {
        size_t run = p->degree - r;
        for (unsigned k = 0; k < T; ++k) {
            const size_t tb = p->taps[k] >> 4;
            if (r >= tb) {
                rp[k] = (uint32_t)(uintptr_t)gather_zero;
            } else {
                if (run > tb - r) run = tb - r;
                const size_t off = (P0 + ((nb + r) << 4) + R - p->taps[k]) & mask;
                const size_t cap = (R - off) >> 4;
                if (run > cap) run = cap;
                rp[k] = (uint32_t)(uintptr_t)(ring + off);
            }
        }
        kernel(tmp + (r << 4), gather_zero, run, rp);
        r += run;
    }
    /* The ring is dead now. A long look-ahead (small300: 4800 bytes) is
     * itself checksummed with the 44-degree polynomial, reusing the ring:
     * its history takes the first half, its own look-ahead the second. */
    const size_t resid = (size_t)p->degree << 4;
    uint32_t pending;
    if (depth == 0 && resid >= MIPS_MMI_GATHER_THRESHOLD)
        pending = gather_run(&gather_chorba352, ~0u, tmp, resid,
                             ring, GATHER_RING_BYTES / 2u,
                             ring + GATHER_RING_BYTES / 2u, 1);
    else
        pending = crc32_braid_mmi_asm(~0u, tmp, resid);
    uint32_t result = ~gather_unshift(p, ~pending);
    if (processed < len)
        result = crc32_braid_mmi_asm(result, buf + processed, len - processed);
    return result;
}

Z_INTERNAL uint32_t crc32_gather_mmi_at(uint32_t crc, const uint8_t *buf,
                                         size_t len, size_t threshold) {
    size_t align = (16u - ((uintptr_t)buf & 15u)) & 15u;
    if (len < threshold + align || len < gather_chorba352.minimum + align)
        return crc32_braid_mmi_asm(crc, buf, len);
    if (align != 0u) {
        crc = crc32_braid_mmi_asm(crc, buf, align);
        buf += align;
        len -= align;
    }
    uint8_t ring[GATHER_RING_BYTES] ALIGNED_(16);
    uint8_t tmp[GATHER_TMP_BLOCKS * 16u] ALIGNED_(16);
    const gather_poly *p = len >= MIPS_MMI_GATHER_LARGE ? &gather_small300 :
                           len >= MIPS_MMI_GATHER_MID ? &gather_t8_66 :
                           &gather_chorba352;
    return gather_run(p, crc, buf, len, ring, GATHER_RING_BYTES, tmp, 0);
}

Z_INTERNAL uint32_t crc32_gather_mmi(uint32_t crc, const uint8_t *buf, size_t len) {
    return crc32_gather_mmi_at(crc, buf, len, MIPS_MMI_GATHER_THRESHOLD);
}

Z_INTERNAL uint32_t crc32_gather_mmi_threshold1024(uint32_t crc, const uint8_t *buf, size_t len) {
    return crc32_gather_mmi_at(crc, buf, len, 1024u);
}
Z_INTERNAL uint32_t crc32_gather_mmi_threshold3072(uint32_t crc, const uint8_t *buf, size_t len) {
    return crc32_gather_mmi_at(crc, buf, len, 3072u);
}

void *zng_mmi_memcpy(void *dst, const void *src, size_t n);

Z_INTERNAL uint32_t crc32_copy_gather_mmi(uint32_t crc, uint8_t *dst,
                                           const uint8_t *src, size_t len) {
    uint32_t result = crc32_gather_mmi(crc, src, len);
    if (len != 0)
        zng_mmi_memcpy(dst, src, len);
    return result;
}

#endif /* MIPS_MMI_CHORBA */
