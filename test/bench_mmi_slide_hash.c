/* Compare serial and interleaved EE MMI slide_hash on the same hardware.
 * Correctness is checked against a scalar reference before timing.
 * clock() can be too coarse on PS2; use an EE cycle counter for final data.
 */
#include "zbuild.h"
#include "deflate.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef MIPS_MMI
#  error "This benchmark requires WITH_MMI=ON"
#endif

void Z_INTERNAL slide_hash_mmi_serial(deflate_state *s);
void Z_INTERNAL slide_hash_mmi_interleaved(deflate_state *s);

static Pos head[HASH_SIZE + 16] ALIGNED_(16);
static Pos prev[32768 + 16] ALIGNED_(16);
static Pos expected_head[HASH_SIZE];
static Pos expected_prev[32768];
static deflate_state state;
static volatile uint32_t sink;

typedef void (*slide_fn)(deflate_state *);

static uint32_t mix(uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    return x ^ (x << 5);
}

static void fill_tables(deflate_state *s) {
    uint32_t seed = 0x9e3779b9u;
    for (uint32_t i = 0; i < HASH_SIZE; ++i) {
        seed = mix(seed);
        s->head[i] = (Pos)seed;
    }
    for (uint32_t i = 0; i < s->w_size; ++i) {
        seed = mix(seed);
        s->prev[i] = (Pos)seed;
    }
}

static void scalar_slide(Pos *array, uint32_t count, Pos wsize) {
    for (uint32_t i = 0; i < count; ++i) {
        Pos value = array[i];
        array[i] = value >= wsize ? value - wsize : 0;
    }
}

static int validate(deflate_state *s, slide_fn fn) {
    Pos wsize = (Pos)s->w_size;
    fill_tables(s);
    memcpy(expected_head, s->head, sizeof(expected_head));
    memcpy(expected_prev, s->prev, (size_t)wsize * sizeof(Pos));
    scalar_slide(expected_head, HASH_SIZE, wsize);
    scalar_slide(expected_prev, wsize, wsize);
    fn(s);
    return memcmp(expected_head, s->head, sizeof(expected_head)) == 0 &&
           memcmp(expected_prev, s->prev, (size_t)wsize * sizeof(Pos)) == 0;
}

static clock_t run(deflate_state *s, slide_fn fn, unsigned iterations) {
    fill_tables(s);
    clock_t before = clock();
    for (unsigned i = 0; i < iterations; ++i)
        fn(s);
    clock_t after = clock();
    sink ^= s->head[0] ^ s->prev[0];
    if (before == (clock_t)-1 || after == (clock_t)-1)
        return (clock_t)-1;
    return after - before;
}

int main(void) {
    static const unsigned offsets[] = {0, 1, 7};
    static const unsigned sizes[] = {1024, 32768};
    const unsigned iterations = 500;

    printf("MMI slide_hash A/B CLOCKS_PER_SEC=%lu\n",
           (unsigned long)CLOCKS_PER_SEC);
    puts("wsize offset iterations serial_ticks interleaved_ticks serial_over_interleaved");
    for (unsigned si = 0; si < sizeof(sizes)/sizeof(sizes[0]); ++si) {
        for (unsigned oi = 0; oi < sizeof(offsets)/sizeof(offsets[0]); ++oi) {
            unsigned offset = offsets[oi];
            state.w_size = sizes[si];
            state.head = head + offset;
            state.prev = prev + offset;
            if (!validate(&state, slide_hash_mmi_serial) ||
                !validate(&state, slide_hash_mmi_interleaved)) {
                printf("MMI slide_hash A/B FAIL wsize=%u offset=%u\n",
                       sizes[si], offset);
                return 1;
            }
            clock_t serial, interleaved;
            if ((si + oi) & 1u) {
                interleaved = run(&state, slide_hash_mmi_interleaved, iterations);
                serial = run(&state, slide_hash_mmi_serial, iterations);
            } else {
                serial = run(&state, slide_hash_mmi_serial, iterations);
                interleaved = run(&state, slide_hash_mmi_interleaved, iterations);
            }
            if (serial <= 0 || interleaved <= 0) {
                printf("%u %u %u clock_unavailable_or_too_coarse\n",
                       sizes[si], offset, iterations);
            } else {
                printf("%u %u %u %ld %ld %.3f\n", sizes[si], offset, iterations,
                       (long)serial, (long)interleaved,
                       (double)serial / (double)interleaved);
            }
        }
    }
    printf("benchmark sink=%lu\n", (unsigned long)sink);
    return 0;
}
