/* test_sparc_vis1_models.c -- portable models for the VIS1 algorithms.
 * This test runs on any host; it does NOT execute VIS instructions.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "arch/sparc/sparc_features.h"

#define ADLER_BASE 65521u
#define ADLER_NMAX 5552u

static uint32_t adler_reference(uint32_t seed, const uint8_t *p, size_t len) {
    uint64_t a = seed & 0xffffu;
    uint64_t b = seed >> 16;
    for (size_t i = 0; i < len; i++) {
        a += p[i];
        b += a;
    }
    return (uint32_t)(a % ADLER_BASE) | ((uint32_t)(b % ADLER_BASE) << 16);
}

/* Mirrors the four 16-bit VIS1 fpmerge/fpadd16 lane sums and their
 * positional weights, not the actual instruction encoding. */
static uint32_t adler_lane_model(uint32_t seed, const uint8_t *p, size_t len) {
    uint32_t a = seed & 0xffffu;
    uint32_t b = seed >> 16;

    while (len) {
        size_t chunk = len < ADLER_NMAX ? len : ADLER_NMAX;
        size_t left = chunk;
        while (left && ((uintptr_t)p & 3u)) {
            a += *p++;
            b += a;
            --left;
        }
        while (left >= 64) {
            uint32_t lane_sum[4] = { 0, 0, 0, 0 };
            uint32_t lane_prefix[4] = { 0, 0, 0, 0 };
            for (unsigned row = 0; row < 16; row++) {
                for (unsigned lane = 0; lane < 4; lane++) {
                    lane_sum[lane] += p[row * 4 + lane];
                    lane_prefix[lane] += lane_sum[lane];
                }
            }
            uint32_t total = lane_sum[0] + lane_sum[1] +
                             lane_sum[2] + lane_sum[3];
            uint32_t prefixes = lane_prefix[0] + lane_prefix[1] +
                                lane_prefix[2] + lane_prefix[3];
            uint32_t offsets = lane_sum[1] + 2u * lane_sum[2] +
                               3u * lane_sum[3];
            b += 64u * a + 4u * prefixes - offsets;
            a += total;
            p += 64;
            left -= 64;
        }
        while (left) {
            a += *p++;
            b += a;
            --left;
        }
        a %= ADLER_BASE;
        b %= ADLER_BASE;
        len -= chunk;
    }
    return a | (b << 16);
}

static int check_hwcap(void) {
    /* 0x40 is Linux/SPARC BLKINIT, not VIS1. */
    if (ZNG_SPARC_HWCAP_VIS != 0x00002000UL ||
        sparc_hwcap_has_vis1(0) ||
        sparc_hwcap_has_vis1(0x40UL) ||
        !sparc_hwcap_has_vis1(0x2000UL) ||
        !sparc_hwcap_has_vis1(0x2040UL)) {
        fputs("Incorrect Linux/SPARC VIS1 HWCAP detection\n", stderr);
        return 1;
    }
    return 0;
}

static int check_slide_masks(void) {
    /* Matches the 16-entry word mask lookup used by slide_hash_vis1. */
    static const uint64_t masks[16] = {
        UINT64_C(0x0000000000000000), UINT64_C(0x000000000000ffff),
        UINT64_C(0x00000000ffff0000), UINT64_C(0x00000000ffffffff),
        UINT64_C(0x0000ffff00000000), UINT64_C(0x0000ffff0000ffff),
        UINT64_C(0x0000ffffffff0000), UINT64_C(0x0000ffffffffffff),
        UINT64_C(0xffff000000000000), UINT64_C(0xffff00000000ffff),
        UINT64_C(0xffff0000ffff0000), UINT64_C(0xffff0000ffffffff),
        UINT64_C(0xffffffff00000000), UINT64_C(0xffffffff0000ffff),
        UINT64_C(0xffffffffffff0000), UINT64_C(0xffffffffffffffff)
    };
    for (unsigned mask = 0; mask < 16; mask++) {
        for (uint32_t value = 0; value < 65536u; value++) {
            uint64_t packed = 0;
            uint16_t values[4];
            for (unsigned lane = 0; lane < 4; lane++) {
                uint16_t v = (uint16_t)((value & 0x7fffu) |
                    (((mask >> (3u - lane)) & 1u) ? 0x8000u : 0u));
                values[lane] = v;
                packed = (packed << 16) | (uint16_t)(v - 32768u);
            }
            packed &= masks[mask];
            for (unsigned lane = 0; lane < 4; lane++) {
                uint16_t got = (uint16_t)(packed >> ((3u - lane) * 16u));
                uint16_t want = values[lane] >= 32768u ?
                                (uint16_t)(values[lane] - 32768u) : 0;
                if (got != want) {
                    fprintf(stderr,
                            "VIS1 mask model failed: mask=%u value=%u lane=%u\n",
                            mask, value, lane);
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int check_adler(void) {
    static const size_t lengths[] = {
        0, 1, 2, 3, 4, 7, 15, 16, 31, 63, 64, 65,
        127, 128, 255, 256, 5535, 5536, 5551, 5552,
        5553, 8192, 16384, 65536
    };
    static const uint32_t seeds[] = { 1, 0, 0x10203040u, 0xfff0fff0u };
    uint8_t buf[65536 + 8];
    unsigned cases = 0;

    for (unsigned shift = 0; shift < 8; shift++) {
        uint8_t *p = buf + shift;
        for (unsigned pattern = 0; pattern < 3; pattern++) {
            for (size_t i = 0; i < 65536; i++) {
                p[i] = pattern == 0 ? 0xffu : pattern == 1 ? (uint8_t)i :
                       (uint8_t)((i * 41u + (i >> 3) * 23u) & 0xffu);
            }
            for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
                for (size_t j = 0; j < sizeof(seeds) / sizeof(seeds[0]); j++) {
                    uint32_t want = adler_reference(seeds[j], p, lengths[i]);
                    uint32_t got = adler_lane_model(seeds[j], p, lengths[i]);
                    if (want != got) {
                        fprintf(stderr,
                                "VIS1 Adler lane model failed: shift=%u pattern=%u len=%lu seed=%08x want=%08x got=%08x\n",
                                shift, pattern, (unsigned long)lengths[i], seeds[j],
                                want, got);
                        return 1;
                    }
                    cases++;
                }
            }
        }
    }
    printf("VIS1 Adler lane model: %u reference comparisons passed\n", cases);
    return 0;
}

int main(void) {
    if (check_hwcap() || check_slide_masks() || check_adler())
        return 1;
    puts("SPARC VIS1 portable arithmetic models passed (no VIS instruction execution)");
    return 0;
}
