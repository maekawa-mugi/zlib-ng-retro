/* EE MMI Chorba differential test against generic braid CRC-32.
 * Run on actual PlayStation 2 hardware with WITH_MMI_CHORBA=ON. */
#include "zbuild.h"
#include "arch_functions.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef MIPS_MMI_CHORBA
#  error "This test requires WITH_MMI_CHORBA=ON"
#endif

#define CHORBA_TEST_MAX_INPUT (262144u + 16u)
static uint8_t input[CHORBA_TEST_MAX_INPUT] ALIGNED_(16);
static uint8_t copy[CHORBA_TEST_MAX_INPUT] ALIGNED_(16);
static uint8_t snapshot[CHORBA_TEST_MAX_INPUT] ALIGNED_(16);
static uint32_t rng = 0x9e3779b9u;

static uint32_t next_rng(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static int run_case(const uint8_t *src, size_t len, uint32_t seed,
                    unsigned alignment) {
    uint32_t expected = crc32_braid(seed, src, len);
    uint32_t got = crc32_chorba_mmi(seed, src, len);
    uint32_t single = crc32_chorba_mmi_single(seed, src, len);
    uint32_t paired = crc32_chorba_mmi_paired(seed, src, len);
    uint32_t t1024 = crc32_chorba_mmi_threshold1024(seed, src, len);
    uint32_t t4096 = crc32_chorba_mmi_threshold4096(seed, src, len);
    uint32_t t8192 = crc32_chorba_mmi_threshold8192(seed, src, len);
    if (expected != got || expected != single || expected != paired ||
        expected != t1024 || expected != t4096 || expected != t8192) {
        printf("MMI Chorba FAIL len=%lu align=%u seed=%08lx ref=%08lx selected=%08lx single=%08lx paired=%08lx t1024=%08lx t4096=%08lx t8192=%08lx\n",
               (unsigned long)len, alignment, (unsigned long)seed,
               (unsigned long)expected, (unsigned long)got,
               (unsigned long)single, (unsigned long)paired,
               (unsigned long)t1024, (unsigned long)t4096, (unsigned long)t8192);
        return 1;
    }

    memcpy(snapshot, src, len);
    /* Exercise production, original two-pass, and fused copy directly.
     * Even input residues match destination alignment in the fused case,
     * odd residues deliberately mismatch to exercise scalar copy tails. */
    for (unsigned variant = 0; variant < 3; ++variant) {
        unsigned dstoff = variant == 0 ? 0 :
                          variant == 1 ? ((alignment + 5u) & 15u) :
                          (alignment & 1u) ? ((alignment + 9u) & 15u) : alignment;
        uint8_t *dst = copy + dstoff;
        memset(copy, 0xa5, sizeof(copy));
        if (variant == 0)
            got = crc32_copy_chorba_mmi(seed, dst, src, len);
        else if (variant == 1)
            got = crc32_copy_chorba_mmi_twopass(seed, dst, src, len);
        else
            got = crc32_copy_chorba_mmi_fused(seed, dst, src, len);
        if (got != expected || memcmp(dst, src, len) != 0 ||
            dst[len] != 0xa5 || (dstoff && dst[-1] != 0xa5) ||
            memcmp(src, snapshot, len) != 0) {
            printf("MMI Chorba copy FAIL len=%lu align=%u destoff=%u variant=%u\n",
                   (unsigned long)len, alignment, dstoff, variant);
            return 1;
        }
    }

    /* Streaming calls must match one-shot CRC regardless of split. */
    size_t cut = len / 3;
    got = crc32_chorba_mmi(seed, src, cut);
    got = crc32_chorba_mmi(got, src + cut, len - cut);
    if (got != expected) {
        printf("MMI Chorba streaming FAIL len=%lu align=%u\n",
               (unsigned long)len, alignment);
        return 1;
    }
    return 0;
}

int main(void) {
    static const size_t lengths[] = {
        0, 1, 15, 16, 31, 63, 256, 1023, 1024, 1025,
        2048, 4095, 4096, 4097, 8191, 8192, 8193,
        16384, 32768, 65536, 262144
    };
    static const uint32_t seeds[] = {0u, 1u, 0xffffffffu, 0x12345678u};

    for (unsigned pattern = 0; pattern < 4; pattern++) {
        for (unsigned i = 0; i < sizeof(input); i++) {
            uint32_t v = next_rng();
            input[i] = pattern == 0 ? (uint8_t)v :
                       pattern == 1 ? 0xffu :
                       pattern == 2 ? 0u : (uint8_t)i;
        }
        for (unsigned offset = 0; offset < 16; offset++)
            for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
                for (unsigned j = 0; j < sizeof(seeds) / sizeof(seeds[0]); j++)
                    if (run_case(input + offset, lengths[i], seeds[j], offset))
                        return 1;
    }

    puts("MMI Chorba: PASS");
    return 0;
}
