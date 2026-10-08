/* Host-runnable arithmetic model for EE MMI Adler-32.
 * No PS2 instructions are executed. This checks the pair-lane formula,
 * alignment peeling, NMAX bounds, stream splitting and modulo handling.
 *
 * cc -std=c11 -O2 -Wall -Wextra -Werror \\
 *   -o test_mmi_adler32_math test/test_mmi_adler32_math.c
 * ./test_mmi_adler32_math
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "../arch/mips/adler32_mmi_math.h"

#define ADLER_BASE 65521u
#define ADLER_NMAX 5552u

static uint8_t data[65536 + 32];
static uint32_t rng = 0x9e3779b9u;

static uint32_t random32(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static uint32_t reference(uint32_t initial, const uint8_t *src, size_t len) {
    uint32_t s1 = initial & 65535u, s2 = initial >> 16;
    for (size_t i = 0; i < len; i++) {
        s1 += src[i];
        s2 += s1;
        /* Keep the mathematical reference bounded independent of NMAX. */
        s1 %= ADLER_BASE;
        s2 %= ADLER_BASE;
    }
    return s2 << 16 | s1;
}

static uint32_t model(uint32_t initial, const uint8_t *src, size_t len) {
    uint32_t s1 = initial & 65535u, s2 = initial >> 16;
    if (len == 0)
        return initial;
    while (len) {
        size_t n = len < ADLER_NMAX ? len : ADLER_NMAX;
        len -= n;
        while (n && ((uintptr_t)src & 15u)) {
            s1 += *src++;
            s2 += s1;
            --n;
        }
        while (n >= 16) {
            uint16_t pair[8];
            for (unsigned i = 0; i < 8; i++)
                pair[i] = (uint16_t)src[i] + (uint16_t)src[i + 8];
            adler32_mmi_reduce16(&s1, &s2, pair, src);
            src += 16;
            n -= 16;
        }
        while (n) {
            s1 += *src++;
            s2 += s1;
            --n;
        }
        s1 %= ADLER_BASE;
        s2 %= ADLER_BASE;
    }
    return s2 << 16 | s1;
}

static int check(const uint8_t *src, size_t len, uint32_t initial) {
    uint32_t actual = model(initial, src, len);
    uint32_t expected = reference(initial, src, len);
    if (actual != expected) {
        printf("MMI Adler math FAIL len=%lu initial=%08lx actual=%08lx expected=%08lx\n",
               (unsigned long)len, (unsigned long)initial,
               (unsigned long)actual, (unsigned long)expected);
        return 1;
    }
    size_t split = len / 3;
    actual = model(initial, src, split);
    actual = model(actual, src + split, len - split);
    if (actual != expected) {
        printf("MMI Adler math streaming FAIL len=%lu initial=%08lx\n",
               (unsigned long)len, (unsigned long)initial);
        return 1;
    }
    return 0;
}

int main(void) {
    static const size_t lengths[] = {
        0, 1, 7, 15, 16, 17, 31, 32, 63, 64, 127, 128,
        255, 256, 511, 1024, 4095, 5551, 5552, 5553,
        8192, 16384, 32768, 65536
    };
    static const uint32_t initials[] = {
        1u, 0u, 0xffffffffu, 0x12345678u, 0xfff0fff0u
    };
    unsigned total = 0;
    for (unsigned pattern = 0; pattern < 4; pattern++) {
        for (unsigned i = 0; i < sizeof(data); i++) {
            uint32_t v = random32();
            data[i] = pattern == 0 ? (uint8_t)v :
                      pattern == 1 ? 255u :
                      pattern == 2 ? 0u : (uint8_t)i;
        }
        for (unsigned off = 0; off < 16; off++) {
            for (unsigned l = 0; l < sizeof(lengths) / sizeof(lengths[0]); l++) {
                for (unsigned k = 0; k < sizeof(initials) / sizeof(initials[0]); k++) {
                    if (check(data + off, lengths[l], initials[k]))
                        return 1;
                    total++;
                }
            }
        }
    }
    printf("MMI Adler-32 math: PASS (%u cases)\n", total);
    return 0;
}
