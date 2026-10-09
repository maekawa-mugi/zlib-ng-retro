/* Exhaustive first-mismatch and alignment checks for the R5900 MMI
 * comparison routine. Run this EE executable on real PS2 hardware. */
#include "zbuild.h"
#include "deflate.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if !defined(MIPS_MMI)
#  error "This test requires WITH_MMI=ON"
#endif

uint32_t Z_INTERNAL compare256_mmi(const uint8_t *, const uint8_t *);
uint32_t Z_INTERNAL compare256_mmi_plain(const uint8_t *, const uint8_t *);
uint32_t Z_INTERNAL compare256_mmi_swar(const uint8_t *, const uint8_t *);
uint32_t Z_INTERNAL compare256_mmi_hybrid16(const uint8_t *, const uint8_t *);
#ifdef MIPS_MMI_COMPARE64
uint32_t Z_INTERNAL compare256_mmi_prefilter64(const uint8_t *, const uint8_t *);
uint32_t Z_INTERNAL compare256_mmi_prefilter32(const uint8_t *, const uint8_t *);
uint32_t Z_INTERNAL compare256_mmi_prefilter32_swar(const uint8_t *, const uint8_t *);
uint32_t Z_INTERNAL compare256_mmi_prefilter64_swar(const uint8_t *, const uint8_t *);
uint32_t Z_INTERNAL compare256_mmi_hybrid16_pre64(const uint8_t *, const uint8_t *);
#endif

static uint8_t a[288] ALIGNED_(16);
static uint8_t b[288] ALIGNED_(16);

static int check(unsigned ao, unsigned bo, unsigned mismatch) {
    uint8_t *left = a + ao, *right = b + bo;
    for (unsigned i = 0; i < 256; i++) {
        uint8_t value = (uint8_t)(i * 113u + 23u);
        left[i] = value;
        right[i] = value;
    }
    if (mismatch < 256)
        right[mismatch] ^= 0xffu;
    /* Check both schedules against the same input, not only the
     * compile-time-selected public dispatch. */
    uint32_t got = compare256_mmi(left, right);
    uint32_t plain = compare256_mmi_plain(left, right);
    uint32_t swar = compare256_mmi_swar(left, right);
    uint32_t hybrid = compare256_mmi_hybrid16(left, right);
    if (got != mismatch || plain != mismatch ||
        swar != mismatch || hybrid != mismatch) {
        printf("MMI compare256: FAIL ao=%u bo=%u mismatch=%u selected=%u plain=%u swar=%u hybrid=%u\n",
               ao, bo, mismatch, (unsigned)got, (unsigned)plain,
               (unsigned)swar,(unsigned)hybrid);
        return 1;
    }
#ifdef MIPS_MMI_COMPARE64
    uint32_t prefilter = compare256_mmi_prefilter64(left, right);
    uint32_t hybrid_pre64 = compare256_mmi_hybrid16_pre64(left,right);
    uint32_t pre32 = compare256_mmi_prefilter32(left, right);
    uint32_t pre32swar = compare256_mmi_prefilter32_swar(left, right);
    uint32_t prefilter_swar = compare256_mmi_prefilter64_swar(left, right);
    if (hybrid_pre64 != mismatch ||
        prefilter != mismatch || prefilter_swar != mismatch ||
        pre32 != mismatch || pre32swar != mismatch) {
        printf("MMI compare256 prefilter: FAIL ao=%u bo=%u mismatch=%u byte=%u swar=%u\n",
               ao, bo, mismatch, (unsigned)prefilter, (unsigned)prefilter_swar);
        return 1;
    }
#endif
    return 0;
}

int main(void) {
    /* All mismatch indices for every pair of 16-byte alignment residues.
     * Also checks all-equal input (mismatch=256). This exercises scalar
     * peel, aligned MMI, SWAR and the 64-byte prefilter boundaries. */
    unsigned long cases = 0;
    for (unsigned ao = 0; ao < 16; ++ao)
        for (unsigned bo = 0; bo < 16; ++bo)
            for (unsigned mismatch = 0; mismatch <= 256; ++mismatch) {
                if (check(ao, bo, mismatch))
                    return 1;
                ++cases;
            }
    printf("MMI compare256: PASS (%lu offset/mismatch cases)\n", cases);
    return 0;
}
