/* Test the actual MMI slide_hash routines against the scalar reference.
 * Build with WITH_MMI=ON, BUILD_SHARED_LIBS=OFF, and BUILD_TESTING=ON.
 * Execute the resulting EE binary on a PlayStation 2, not an ordinary MIPS CPU.
 */
#include "zbuild.h"
#include "deflate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(MIPS_MMI)
#  error "This test requires an MMI-enabled PS2 EE build"
#endif

void Z_INTERNAL slide_hash_mmi(deflate_state *s);
void Z_INTERNAL slide_hash_head_mmi(deflate_state *s);
void Z_INTERNAL slide_hash_mmi_serial(deflate_state *s);
void Z_INTERNAL slide_hash_mmi_interleaved(deflate_state *s);
void Z_INTERNAL slide_hash_head_mmi_serial(deflate_state *s);
void Z_INTERNAL slide_hash_head_mmi_interleaved(deflate_state *s);

static uint32_t seed = 0x9e3779b9u;

static uint16_t next_value(uint16_t wsize, uint32_t index) {
    uint32_t v;
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    v = seed;
    switch (index & 15u) {
        case 0: return 0;
        case 1: return 1;
        case 2: return (uint16_t)(wsize - 1);
        case 3: return wsize;
        case 4: return (uint16_t)(wsize + 1);
        case 5: return 32767;
        case 6: return 32768;
        case 7: return 65535;
        default: return (uint16_t)v;
    }
}

/* Reserve space before and after the buffer for unaligned-base testing. */
static Pos *aligned_with_offset(void *raw, uint32_t offset) {
    uintptr_t aligned = ((uintptr_t)raw + 31u) & ~(uintptr_t)15u;
    return (Pos *)aligned + offset;
}

static void reference_slide(Pos *table, uint32_t entries, Pos wsize) {
    for (uint32_t i = 0; i < entries; i++) {
        Pos v = table[i];
        table[i] = v >= wsize ? v - wsize : 0;
    }
}

static int first_mismatch(const Pos *a, const Pos *b, uint32_t entries) {
    for (uint32_t i = 0; i < entries; i++)
        if (a[i] != b[i])
            return (int)i;
    return -1;
}

int main(void) {
    static const Pos sizes[] = {1, 8, 256, 1024, 32768};
    size_t head_bytes = (size_t)HASH_SIZE * sizeof(Pos);
    size_t prev_bytes = (size_t)32768 * sizeof(Pos);
    void *hraw = malloc(head_bytes + 64);
    void *praw = malloc(prev_bytes + 64);
    Pos *hexp = (Pos *)malloc(head_bytes);
    Pos *pexp = (Pos *)malloc(prev_bytes);
    Pos *prev_original = (Pos *)malloc(prev_bytes);
    deflate_state *s = (deflate_state *)calloc(1, sizeof(*s));
    if (!hraw || !praw || !hexp || !pexp || !prev_original || !s) {
        puts("MMI slide_hash: allocation failed");
        return 2;
    }

    /* 0 = production selection, 1 = serial baseline, 2 = interleaved. */
    static void (*const full[3])(deflate_state *) = {
        slide_hash_mmi, slide_hash_mmi_serial, slide_hash_mmi_interleaved
    };
    static void (*const head_only[3])(deflate_state *) = {
        slide_hash_head_mmi, slide_hash_head_mmi_serial,
        slide_hash_head_mmi_interleaved
    };

    for (unsigned size_index = 0; size_index < sizeof(sizes) / sizeof(sizes[0]); size_index++) {
        Pos wsize = sizes[size_index];
        for (unsigned offset = 0; offset < 8; offset++) {
            for (unsigned variant = 0; variant < 3; variant++) {
                s->w_size = wsize;
                s->head = aligned_with_offset(hraw, offset);
                s->prev = aligned_with_offset(praw, (offset + 3u) & 7u);

                /* Ensure the vector fast path never alters bytes outside the table. */
                s->head[-1] = 0xa55a;
                s->head[HASH_SIZE] = 0x5aa5;
                s->prev[-1] = 0xa55a;
                s->prev[wsize] = 0x5aa5;

                for (uint32_t i = 0; i < HASH_SIZE; i++)
                    s->head[i] = next_value(wsize, i);
                for (uint32_t i = 0; i < wsize; i++)
                    s->prev[i] = next_value(wsize, i);

                memcpy(hexp, s->head, head_bytes);
                memcpy(pexp, s->prev, (size_t)wsize * sizeof(Pos));
                memcpy(prev_original, s->prev, (size_t)wsize * sizeof(Pos));
                reference_slide(hexp, HASH_SIZE, wsize);
                reference_slide(pexp, wsize, wsize);

                full[variant](s);
                if (first_mismatch(s->head, hexp, HASH_SIZE) >= 0 ||
                    first_mismatch(s->prev, pexp, wsize) >= 0 ||
                    s->head[-1] != 0xa55a || s->head[HASH_SIZE] != 0x5aa5 ||
                    s->prev[-1] != 0xa55a || s->prev[wsize] != 0x5aa5) {
                    printf("MMI slide_hash failed: wsize=%u offset=%u variant=%u\n", (unsigned)wsize, offset, variant);
                    return 1;
                }

                /* Reconstruct input and verify head-only sliding does not touch prev. */
                for (uint32_t i = 0; i < HASH_SIZE; i++)
                    s->head[i] = next_value(wsize, i);
                memcpy(hexp, s->head, head_bytes);
                reference_slide(hexp, HASH_SIZE, wsize);
                memcpy(s->prev, prev_original, (size_t)wsize * sizeof(Pos));

                head_only[variant](s);
                if (first_mismatch(s->head, hexp, HASH_SIZE) >= 0 ||
                    first_mismatch(s->prev, prev_original, wsize) >= 0 ||
                    s->head[-1] != 0xa55a || s->head[HASH_SIZE] != 0x5aa5 ||
                    s->prev[-1] != 0xa55a || s->prev[wsize] != 0x5aa5) {
                    printf("MMI head-only failed: wsize=%u offset=%u variant=%u\n", (unsigned)wsize, offset, variant);
                    return 1;
                }
            }
        }
    }
    puts("MMI slide_hash: PASS (selected, serial, interleaved)");
    free(hraw);
    free(praw);
    free(hexp);
    free(pexp);
    free(prev_original);
    free(s);
    return 0;
}
