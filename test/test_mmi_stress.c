/* Deterministic random differential stress test for PS2 EE MMI kernels.
 * Runs on actual PS2 Linux. Emits first failure case and seed for replay.
 * In addition to fixed regressions, probes alignments, lengths, CRC seeds,
 * forward and overlapping LZ77, multiple compare strategies and sentinels.
 */
#include "zbuild.h"
#include "arch_functions.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef MIPS_MMI
#  error "Requires WITH_MMI=ON"
#endif

#define STRESS_COMPARE 5000u
#define STRESS_COPY 3000u
#define STRESS_CHECKSUM 250u
#define STRESS_CRC 64u

static uint32_t rng = 0xa64d29b7u;
static uint32_t random32(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static uint8_t cmpa[288] ALIGNED_(16), cmpb[288] ALIGNED_(16);
static uint8_t actual[4096] ALIGNED_(16), expected[4096] ALIGNED_(16);
#ifdef MIPS_MMI_ADLER32
static uint8_t adler_src[32768 + 32] ALIGNED_(16);
static uint8_t adler_dst[32768 + 32] ALIGNED_(16);
#endif
#ifdef MIPS_MMI_CHORBA
static uint8_t crc_src[32768 + 32] ALIGNED_(16);
static uint8_t crc_dst[32768 + 32] ALIGNED_(16);
#endif

typedef uint32_t (*compare_fn)(const uint8_t *, const uint8_t *);
typedef uint8_t *(*copy_fn)(uint8_t *, uint8_t *, size_t, size_t);

static int test_comparisons(void) {
    static const compare_fn variants[] = {
        compare256_c, compare256_mmi_plain, compare256_mmi_swar,
#ifdef MIPS_MMI_COMPARE64
        compare256_mmi_prefilter64, compare256_mmi_prefilter64_swar,
#endif
        compare256_mmi
    };
    for (unsigned t = 0; t < STRESS_COMPARE; ++t) {
        unsigned ao = random32() & 15u, bo = random32() & 15u;
        unsigned mismatch = random32() % 257u;
        uint8_t *a = cmpa + ao, *b = cmpb + bo;
        for (unsigned i = 0; i < 256; ++i)
            a[i] = b[i] = (uint8_t)random32();
        if (mismatch < 256)
            b[mismatch] ^= (uint8_t)(1u << (random32() & 7u));
        for (unsigned variant = 0; variant < sizeof(variants)/sizeof(variants[0]); ++variant) {
            uint32_t got = variants[variant](a, b);
            if (got != mismatch) {
                printf("MMI stress compare FAIL trial=%u ao=%u bo=%u mismatch=%u variant=%u got=%u seed=%08lx\n",
                       t, ao, bo, mismatch, variant, got, (unsigned long)rng);
                return 1;
            }
        }
    }
    return 0;
}

static int test_copies(void) {
    static const copy_fn variants[] = {
        chunkmemset_safe_c, chunkmemset_safe_mmi_serial,
        chunkmemset_safe_mmi_burst, chunkmemset_safe_mmi_pattern,
        chunkmemset_safe_mmi
    };
    for (unsigned trial = 0; trial < STRESS_COPY; ++trial) {
        unsigned offset = random32() & 15u;
        unsigned distance = 1u + (random32() % 256u);
        unsigned length = random32() % 513u;
        unsigned left = random32() % 513u;
        const unsigned n = length < left ? length : left;
        const unsigned outpos = 1024u + offset;
        for (unsigned i = 0; i < sizeof(actual); ++i)
            actual[i] = expected[i] = (uint8_t)random32();
        for (unsigned i = 0; i < n; ++i)
            expected[outpos + i] = expected[outpos + i - distance];
        for (unsigned variant = 0; variant < sizeof(variants)/sizeof(variants[0]); ++variant) {
            for (unsigned i = 0; i < sizeof(actual); ++i)
                actual[i] = expected[i];
            /* Reconstruct original source/destination without carrying
             * reference modifications into the candidate buffer. */
            for (unsigned i = 0; i < sizeof(actual); ++i)
                actual[i] = (uint8_t)(i * 31u + 7u);
            for (unsigned i = 0; i < sizeof(actual); ++i)
                expected[i] = actual[i];
            for (unsigned i = 0; i < n; ++i)
                expected[outpos + i] = expected[outpos + i - distance];

            uint8_t *end = variants[variant](actual + outpos,
                                               actual + outpos - distance,
                                               length, left);
            if (end != actual + outpos + n ||
                memcmp(actual, expected, sizeof(actual)) != 0) {
                printf("MMI stress copy FAIL trial=%u dist=%u len=%u left=%u offset=%u variant=%u seed=%08lx\n",
                       trial, distance, length, left, offset, variant,
                       (unsigned long)rng);
                return 1;
            }
        }
    }
    return 0;
}

#ifdef MIPS_MMI_ADLER32
static int test_adler(void) {
    for (unsigned i = 0; i < sizeof(adler_src); ++i)
        adler_src[i] = (uint8_t)random32();
    for (unsigned trial = 0; trial < STRESS_CHECKSUM; ++trial) {
        size_t length = random32() % 32769u;
        unsigned src_offset = random32() & 15u;
        unsigned dst_offset = random32() & 15u;
        uint32_t initial = random32();
        const uint8_t *src = adler_src + src_offset;
        uint8_t *dst = adler_dst + dst_offset;
        uint32_t expected_crc = adler32_c(initial, src, length);
        if (adler32_mmi(initial, src, length) != expected_crc ||
            adler32_mmi_prefix(initial, src, length) != expected_crc ||
            adler32_mmi_formula(initial, src, length) != expected_crc) {
            printf("MMI stress Adler checksum FAIL trial=%u len=%lu seed=%08lx\n",
                   trial, (unsigned long)length, (unsigned long)rng);
            return 1;
        }
        memset(adler_dst, 0xa5, sizeof(adler_dst));
        if (adler32_copy_mmi_fused(initial, dst, src, length) != expected_crc ||
            memcmp(dst, src, length) != 0 || dst[length] != 0xa5) {
            printf("MMI stress Adler fused copy FAIL trial=%u len=%lu seed=%08lx\n",
                   trial, (unsigned long)length, (unsigned long)rng);
            return 1;
        }
    }
    return 0;
}
#endif

#ifdef MIPS_MMI_CHORBA
static int test_crc(void) {
    for (unsigned i = 0; i < sizeof(crc_src); ++i)
        crc_src[i] = (uint8_t)random32();
    for (unsigned trial = 0; trial < STRESS_CRC; ++trial) {
        size_t length = random32() % 32769u;
        unsigned src_offset = random32() & 15u;
        unsigned dst_offset = random32() & 15u;
        uint32_t initial = random32();
        const uint8_t *src = crc_src + src_offset;
        uint8_t *dst = crc_dst + dst_offset;
        uint32_t reference = crc32_braid(initial, src, length);
        if (crc32_chorba_mmi(initial, src, length) != reference ||
            crc32_chorba_mmi_single(initial, src, length) != reference ||
            crc32_chorba_mmi_paired(initial, src, length) != reference) {
            printf("MMI stress Chorba FAIL trial=%u len=%lu seed=%08lx\n",
                   trial, (unsigned long)length, (unsigned long)rng);
            return 1;
        }
        memset(crc_dst, 0xa5, sizeof(crc_dst));
        if (crc32_copy_chorba_mmi_fused(initial, dst, src, length) != reference ||
            memcmp(dst, src, length) != 0 || dst[length] != 0xa5) {
            printf("MMI stress Chorba fused copy FAIL trial=%u len=%lu seed=%08lx\n",
                   trial, (unsigned long)length, (unsigned long)rng);
            return 1;
        }
    }
    return 0;
}
#endif

int main(void) {
    if (test_comparisons() || test_copies())
        return 1;
#ifdef MIPS_MMI_ADLER32
    if (test_adler())
        return 1;
#endif
#ifdef MIPS_MMI_CHORBA
    if (test_crc())
        return 1;
#endif
    printf("MMI stress: PASS (compare=%u copy=%u",
           STRESS_COMPARE, STRESS_COPY);
#ifdef MIPS_MMI_ADLER32
    printf(" adler=%u", STRESS_CHECKSUM);
#endif
#ifdef MIPS_MMI_CHORBA
    printf(" chorba=%u", STRESS_CRC);
#endif
    puts(")");
    return 0;
}
