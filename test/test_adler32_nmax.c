/* Alignment peeling must not extend an unreduced Adler block beyond NMAX.
 * Standalone host check (after generating zlib-ng headers with CMake):
 * cc -std=c11 -O2 -Ibuild -I. test/test_adler32_nmax.c \
 *    arch/generic/adler32_c.c -o test_adler32_nmax
 */
#include "zbuild.h"
#include "arch_functions.h"

static uint8_t input[2 * 5552 + 32] ALIGNED_(16);
static uint8_t output[sizeof(input)];

static uint32_t reference(uint32_t initial, const uint8_t *src, size_t len) {
    uint32_t a = initial & 65535u, b = initial >> 16;
    while (len--) {
        a = (a + *src++) % 65521u;
        b = (b + a) % 65521u;
    }
    return ((b % 65521u) << 16) | (a % 65521u);
}

int main(void) {
    const uint32_t seeds[] = {1u, 0u, 0xffffffffu, 0xfff0fff0u};
    unsigned cases = 0;
    for (unsigned pattern = 0; pattern < 3; ++pattern) {
        for (size_t i = 0; i < sizeof(input); ++i)
            input[i] = pattern == 0 ? 255u : pattern == 1 ? (uint8_t)i : 0u;
        for (unsigned offset = 0; offset < 16; ++offset) {
            for (size_t boundary = 0; boundary <= 2 * 5552; boundary += 5552) {
                size_t first = boundary ? boundary - 16 : 0;
                for (size_t len = first; len <= boundary + 16; ++len) {
                    for (unsigned seed = 0; seed < sizeof(seeds) / sizeof(seeds[0]); ++seed) {
                        uint32_t expected = reference(seeds[seed], input + offset, len);
                        uint32_t actual = adler32_c(seeds[seed], input + offset, len);
                        uint32_t copied = adler32_copy_c(seeds[seed], output, input + offset, len);
                        size_t split = len / 3;
                        uint32_t streamed = adler32_c(seeds[seed], input + offset, split);
                        streamed = adler32_c(streamed, input + offset + split, len - split);
                        if (actual != expected || copied != expected || streamed != expected ||
                            memcmp(output, input + offset, len) != 0) {
                            printf("Adler NMAX FAIL offset=%u len=%lu initial=%08lx got=%08lx copy=%08lx stream=%08lx expected=%08lx\n",
                                   offset, (unsigned long)len, (unsigned long)seeds[seed],
                                   (unsigned long)actual, (unsigned long)copied,
                                   (unsigned long)streamed, (unsigned long)expected);
                            return 1;
                        }
                        ++cases;
                    }
                }
            }
        }
    }
    printf("Adler NMAX: PASS (%u cases)\n", cases);
    return 0;
}
