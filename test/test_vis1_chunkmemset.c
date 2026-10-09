/* Regression checks for SPARC aligned 64-bit LZ77 copy and scalar fallback.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif
#include "zbuild.h"
#include "arch/sparc/sparc_features.h"

#if defined(HAVE_SPARC_GETAUXVAL)
#  include <sys/auxv.h>
#  include <elf.h>
#endif

uint8_t *chunkmemset_safe_sparc(uint8_t *out, uint8_t *from, size_t len,
                                size_t left);

static void fill(uint8_t *buf, size_t size, unsigned seed) {
    for (size_t i = 0; i < size; ++i)
        buf[i] = (uint8_t)(i * 73u + (i >> 3) * 19u + seed * 41u);
}

int main(void) {
    static const unsigned distances[] = {
        1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 24, 31, 32, 40, 48, 63, 64, 80
    };
    static const size_t lengths[] = {
        0, 1, 2, 3, 7, 8, 9, 15, 16, 23, 24, 31, 32, 33,
        39, 40, 63, 64, 65, 95, 96, 127, 128, 191, 255
    };
    uint8_t real[1024];
    uint8_t oracle[1024];
    unsigned cases = 0;

#if defined(HAVE_SPARC_GETAUXVAL) && !defined(DISABLE_RUNTIME_CPU_DETECTION)
    if (!sparc_hwcap_has_vis1(getauxval(AT_HWCAP))) {
        puts("VIS1 is absent: skipping direct SPARC chunk copy test");
        return 77; /* CTest SKIP_RETURN_CODE */
    }
#endif

    for (unsigned offset = 0; offset < 8; ++offset) {
        const unsigned out_index = 128 + offset;
        for (unsigned di = 0; di < sizeof(distances) / sizeof(distances[0]); ++di) {
            const unsigned dist = distances[di];
            for (unsigned li = 0; li < sizeof(lengths) / sizeof(lengths[0]); ++li) {
                const size_t len = lengths[li];
                for (unsigned mode = 0; mode < 4; ++mode) {
                    const size_t left = mode == 0 ? len :
                                        mode == 1 ? len / 2 :
                                        mode == 2 ? 8 : 260;
                    const size_t actual = MIN(len, left);
                    fill(real, sizeof(real), mode + di);
                    memcpy(oracle, real, sizeof(real));

                    /* Reference must copy forwards for LZ77 overlap. */
                    for (size_t i = 0; i < actual; ++i)
                        oracle[out_index + i] = oracle[out_index - dist + i];

                    uint8_t *end = chunkmemset_safe_sparc(
                        real + out_index, real + out_index - dist, len, left);

                    if (end != real + out_index + actual ||
                        memcmp(real + out_index, oracle + out_index, actual) != 0 ||
                        memcmp(real, oracle, out_index) != 0 ||
                        memcmp(real + out_index + left, oracle + out_index + left,
                               sizeof(real) - out_index - left) != 0) {
                        fprintf(stderr, "VIS1 LZ copy: offset=%u dist=%u len=%lu left=%lu\n",
                                offset, dist, (unsigned long)len, (unsigned long)left);
                        return 1;
                    }
                    ++cases;
                }
            }
        }
    }
    printf("SPARC chunk copy: %u cases passed\n", cases);
    return 0;
}
