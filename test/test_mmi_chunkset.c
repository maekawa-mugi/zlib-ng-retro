/* Compare the native EE copy with a byte-accurate LZ77 reference.
 * Execute on actual PlayStation 2 hardware. */
#include "zbuild.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if !defined(MIPS_MMI)
#  error "This test requires WITH_MMI=ON"
#endif

uint8_t *Z_INTERNAL chunkmemset_safe_mmi(uint8_t *, uint8_t *, size_t, size_t);

static uint8_t actual[1024] ALIGNED_(16);
static uint8_t expected[1024] ALIGNED_(16);

static int run(unsigned direction, unsigned dist, unsigned length,
               unsigned left, unsigned offset) {
    const unsigned outpos = 384 + offset;
    const unsigned frompos = direction == 0 ? outpos - dist : outpos + dist;
    const unsigned n = length < left ? length : left;
    for (unsigned i = 0; i < sizeof(actual); i++)
        actual[i] = expected[i] = (uint8_t)(i * 43u + (i >> 3) * 17u);

    uint8_t *end = chunkmemset_safe_mmi(actual + outpos, actual + frompos,
                                        length, left);
    if (direction == 1 && dist < n) {
        memmove(expected + outpos, expected + frompos, n);
    } else {
        /* LZ77 history copies are sequential: dist < len repeats bytes. */
        for (unsigned i = 0; i < n; i++)
            expected[outpos + i] = expected[frompos + i];
    }

    if (end != actual + outpos + n || memcmp(actual, expected, sizeof(actual))) {
        printf("MMI copy: FAIL direction=%u dist=%u len=%u left=%u offset=%u\n",
               direction, dist, length, left, offset);
        return 1;
    }
    return 0;
}

int main(void) {
    static const unsigned dists[] = {1, 2, 3, 4, 7, 8, 15, 16, 17, 31,
                                     32, 48, 63, 64, 96, 127, 128, 192, 256};
    static const unsigned lens[] = {0, 1, 7, 8, 15, 16, 17, 31, 32,
                                    33, 63, 64, 65, 127, 128, 191, 256};
    for (unsigned direction = 0; direction <= 1; direction++)
        for (unsigned d = 0; d < sizeof(dists)/sizeof(dists[0]); d++)
            for (unsigned l = 0; l < sizeof(lens)/sizeof(lens[0]); l++)
                for (unsigned offset = 0; offset < 16; offset++) {
                    unsigned len = lens[l];
                    if (run(direction, dists[d], len, len, offset) ||
                        run(direction, dists[d], len, len/2, offset))
                        return 1;
                }
    puts("MMI chunkmemset_safe: PASS");
    return 0;
}
