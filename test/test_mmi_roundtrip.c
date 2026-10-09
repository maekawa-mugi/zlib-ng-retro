/* PS2 EE end-to-end compression/decompression regression test.
 * Tests multiple compression levels, long LZ77 back-references, and window
 * sliding. Run the generated binary on actual PlayStation 2 hardware.
 */
#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(MIPS_MMI)
#  error "This test requires WITH_MMI=ON"
#endif

#define TEST_MAX (256u * 1024u)
static uint8_t input[TEST_MAX];
static uint8_t output[TEST_MAX];

static void fill_input(unsigned pattern, size_t len) {
    uint32_t state = 0x12345678u;
    for (size_t i = 0; i < len; i++) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        switch (pattern) {
            case 0: input[i] = (uint8_t)state; break;
            case 1: input[i] = (uint8_t)(i & 15u); break;
            case 2: input[i] = 0; break;
            default:
                /* Mix random regions with long repeated sequences to
                 * exercise both deflate hash sliding and inflate copying. */
                input[i] = ((i / 4096u) & 1u) ?
                    (uint8_t)(i % 31u) : (uint8_t)(state & 255u);
                break;
        }
    }
}

int main(void) {
    static const unsigned levels[] = {1, 6, 9};
    static const size_t lengths[] = {0, 1, 16, 257, 4096,
                                     32768, 65536, 98304, TEST_MAX};
    /* Quick strategy at level 1 can expand incompressible data by much
     * more than 1KB. Use the library's bound for all tested levels. */
    z_uintmax_t capacity = PREFIX(compressBound)(TEST_MAX);
    uint8_t *compressed = malloc((size_t)capacity);
    if (compressed == NULL) {
        puts("MMI roundtrip: output allocation FAIL");
        return 1;
    }
    for (unsigned pattern = 0; pattern < 4; pattern++) {
        for (unsigned li = 0; li < sizeof(lengths)/sizeof(lengths[0]); li++) {
            size_t len = lengths[li];
            fill_input(pattern, len);
            for (unsigned level_i = 0; level_i < sizeof(levels)/sizeof(levels[0]); level_i++) {
                int level = (int)levels[level_i];
                z_uintmax_t dest_len = capacity;
                z_uintmax_t decoded_len = (z_uintmax_t)sizeof(output);
                int status = PREFIX(compress2)(compressed, &dest_len,
                                                input, (z_uintmax_t)len, level);
                if (status != Z_OK) {
                    printf("MMI roundtrip: COMPRESS FAIL pattern=%u len=%lu level=%d status=%d\n",
                           pattern, (unsigned long)len, level, status);
                    free(compressed);
                    return 1;
                }
                memset(output, 0xa5, sizeof(output));
                status = PREFIX(uncompress)(output, &decoded_len, compressed, dest_len);
                if (status != Z_OK || (size_t)decoded_len != len ||
                    memcmp(input, output, len) != 0) {
                    printf("MMI roundtrip: DECOMPRESS FAIL pattern=%u len=%lu level=%d status=%d out=%lu\n",
                           pattern, (unsigned long)len, level, status,
                           (unsigned long)decoded_len);
                    free(compressed);
                    return 1;
                }
            }
        }
    }
    free(compressed);
    puts("MMI roundtrip: PASS");
    return 0;
}
