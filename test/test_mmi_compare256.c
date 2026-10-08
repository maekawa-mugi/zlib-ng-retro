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
    uint32_t got = compare256_mmi(left, right);
    if (got != mismatch) {
        printf("MMI compare256: FAIL ao=%u bo=%u mismatch=%u got=%u\n",
               ao, bo, mismatch, got);
        return 1;
    }
    return 0;
}

int main(void) {
    /* Misaligned pairs exercise the scalar fallback. Pairs with equal
     * residues also exercise the scalar-to-vector transition. */
    for (unsigned ao = 0; ao < 16; ao++)
        for (unsigned bo = 0; bo < 16; bo++) {
            static const unsigned indices[] = {
                0, 1, 2, 7, 8, 14, 15, 16, 17, 31, 32, 63, 64,
                127, 128, 191, 239, 254, 255, 256
            };
            for (unsigned i = 0; i < sizeof(indices)/sizeof(indices[0]); i++)
                if (check(ao, bo, indices[i]))
                    return 1;
        }

    for (unsigned mismatch = 0; mismatch <= 256; mismatch++) {
        if (check(0, 0, mismatch) || check(1, 1, mismatch))
            return 1;
    }
    puts("MMI compare256: PASS");
    return 0;
}
