/* Test SPARC VIS1 saturated 16-bit slide_hash on both tables.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif
#include "zbuild.h"
#include "arch/sparc/sparc_features.h"
#include "deflate.h"

#if defined(HAVE_SPARC_GETAUXVAL)
#  include <sys/auxv.h>
#  include <elf.h>
#endif

void slide_hash_vis1(deflate_state *s);
void slide_hash_head_vis1(deflate_state *s);

static uint16_t input_position(uint32_t index, unsigned pattern) {
    const uint16_t boundary[] = {0, 1, 16383, 16384, 32767, 32768, 32769, 65534, 65535};
    if (pattern == 0)
        return boundary[index % (sizeof(boundary) / sizeof(boundary[0]))];
    if (pattern == 1)
        return (uint16_t)(index * 25253u + (index >> 2) * 907u);
    if (pattern == 2)
        return 65535;
    if (pattern == 3)
        return 0;
    /* Pattern 4 enumerates all 16 fcmpgt16 lane masks. The highest
     * 16-bit lane corresponds to mask bit 3, and the lowest to bit 0. */
    unsigned group = (index / 4) % 16;
    unsigned lane = index % 4;
    uint16_t low = (uint16_t)((index * 953u) & 0x7fffu);
    return (uint16_t)(low | (((group >> (3 - lane)) & 1u) ? 0x8000u : 0));
}

static uint16_t expected_position(uint16_t value, uint16_t wsize) {
    return value >= wsize ? (uint16_t)(value - wsize) : 0;
}

int main(void) {
    /* Extra entries permit offsets that are 2-byte aligned, but not
     * necessarily 8-byte aligned, for each of the two hash arrays. */
    static Pos head_data[HASH_SIZE + 8] ALIGNED_(8);
    static Pos prev_data[32768 + 8] ALIGNED_(8);
    static Pos reference_head[HASH_SIZE] ALIGNED_(8);
    static Pos reference_prev[32768] ALIGNED_(8);
    deflate_state s;
    unsigned cases = 0;

#if defined(HAVE_SPARC_GETAUXVAL) && !defined(DISABLE_RUNTIME_CPU_DETECTION)
    if (!sparc_hwcap_has_vis1(getauxval(AT_HWCAP))) {
        puts("VIS1 not present: skipping direct VIS1 slide_hash tests");
        return 77; /* CTest SKIP_RETURN_CODE */
    }
#endif

    for (unsigned head_shift = 0; head_shift < 4; ++head_shift) {
        for (unsigned prev_shift = 0; prev_shift < 4; ++prev_shift) {
            for (unsigned pattern = 0; pattern < 5; ++pattern) {
                for (unsigned wcase = 0; wcase < 2; ++wcase) {
                    const uint16_t wsize = wcase ? 16384 : 32768;
                    Pos *head = head_data + head_shift;
                    Pos *prev = prev_data + prev_shift;
                    memset(&s, 0, sizeof(s));
                    s.head = head;
                    s.prev = prev;
                    s.w_size = wsize;

                    /* Guard the start/end of both arrays, including
                     * tests where the pointers are not 8-byte aligned. */
                    if (head_shift)
                        head[-1] = 0x5a5au;
                    if (prev_shift)
                        prev[-1] = 0xa5a5u;
                    head[HASH_SIZE] = 0x1234u;
                    prev[wsize] = 0x5678u;

                    for (uint32_t i = 0; i < HASH_SIZE; ++i) {
                        head[i] = input_position(i, pattern);
                        reference_head[i] = expected_position(head[i], wsize);
                    }
                    for (uint32_t i = 0; i < wsize; ++i) {
                        prev[i] = input_position(i + 31, pattern);
                        reference_prev[i] = expected_position(prev[i], wsize);
                    }

                    slide_hash_head_vis1(&s);
                    if (memcmp(head, reference_head, sizeof(reference_head)) != 0) {
                        fprintf(stderr, "slide_hash_head failed: shift=%u pattern=%u wsize=%u\n",
                                head_shift, pattern, wsize);
                        return 1;
                    }
                    for (uint32_t i = 0; i < wsize; ++i) {
                        if (prev[i] != input_position(i + 31, pattern)) {
                            fputs("slide_hash_head modified prev table\n", stderr);
                            return 1;
                        }
                    }
                    /* Refill head because the full variant slides it too. */
                    for (uint32_t i = 0; i < HASH_SIZE; ++i)
                        head[i] = input_position(i, pattern);

                    slide_hash_vis1(&s);
                    if (memcmp(head, reference_head, sizeof(reference_head)) != 0 ||
                        memcmp(prev, reference_prev, wsize * sizeof(Pos)) != 0) {
                        fprintf(stderr, "slide_hash failed: shifts=%u,%u pattern=%u wsize=%u\n",
                                head_shift, prev_shift, pattern, wsize);
                        return 1;
                    }
                    if ((head_shift && head[-1] != 0x5a5au) ||
                        (prev_shift && prev[-1] != 0xa5a5u) ||
                        head[HASH_SIZE] != 0x1234u || prev[wsize] != 0x5678u) {
                        fputs("VIS1 slide_hash overwrote a table guard\n", stderr);
                        return 1;
                    }
                    ++cases;
                }
            }
        }
    }

    printf("VIS1 slide_hash: %u cases passed\n", cases);
    return 0;
}
