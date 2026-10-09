/* R5900 EE: table-free CRC32 zero-polynomial MMI study (bench only).
 *
 * Polynomial candidates from Sam Russell, "Chorba: A novel CRC32
 * implementation", arXiv:2412.16398, Table in Section V.
 * The degree-5869 polynomial is RECIPROCATED from the paper's
 * standard-CRC32 orientation to reflected CRC32.
 *
 * All polynomials are checked offline to be divisible by the reflected
 * CRC32 generator (0x1db710641). Offsets are multiplied by 16 bytes:
 * raising each zero polynomial to its 128th power over GF(2).
 *
 * This file is linked ONLY into the PS2 MMI suite executable.
 * Production zlib-ng CRC32 dispatch is never changed.
 * No SPR (0x70000000) access, and no DMA.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern uint32_t crc32_braid(uint32_t crc, const uint8_t *buf, size_t len);

typedef struct {
    const char *name;
    uint32_t degree, ring_bytes, minimum;
    uint32_t tap_count;
    uint32_t taps[14];
    uint32_t unshift[32];
} crc_poly_spec;

static const crc_poly_spec polynomials[] = {
    { "gen32", 32u, 1024u, 1040u, 14u,
      { 96u, 144u, 160u, 256u, 320u, 336u, 352u, 384u, 400u, 432u, 448u, 480u, 496u, 512u },
      {
        0x9df58cb7u, 0xe09a1f2fu, 0x1a45381fu, 0x348a703eu,
        0x6914e07cu, 0xd229c0f8u, 0x7f2287b1u, 0xfe450f62u,
        0x27fb1885u, 0x4ff6310au, 0x9fec6214u, 0xe4a9c269u,
        0x12228293u, 0x24450526u, 0x488a0a4cu, 0x91141498u,
        0xf9592f71u, 0x29c358a3u, 0x5386b146u, 0xa70d628cu,
        0x956bc359u, 0xf1a680f3u, 0x383c07a7u, 0x70780f4eu,
        0xe0f01e9cu, 0x1a913b79u, 0x352276f2u, 0x6a44ede4u,
        0xd489dbc8u, 0x7262b1d1u, 0xe4c563a2u, 0x12fbc105u
      } },
    { "chorba352", 44u, 1024u, 1424u, 10u,
      { 16u, 48u, 112u, 144u, 192u, 208u, 448u, 592u, 624u, 704u, 0u, 0u, 0u, 0u },
      {
        0x65ae920eu, 0xcb5d241cu, 0x4dcb4e79u, 0x9b969cf2u,
        0xec5c3fa5u, 0x03c9790bu, 0x0792f216u, 0x0f25e42cu,
        0x1e4bc858u, 0x3c9790b0u, 0x792f2160u, 0xf25e42c0u,
        0x3fcd83c1u, 0x7f9b0782u, 0xff360f04u, 0x251d1849u,
        0x4a3a3092u, 0x94746124u, 0xf399c409u, 0x3c428e53u,
        0x78851ca6u, 0xf10a394cu, 0x396574d9u, 0x72cae9b2u,
        0xe595d364u, 0x105aa089u, 0x20b54112u, 0x416a8224u,
        0x82d50448u, 0xdedb0ed1u, 0x66c71be3u, 0xcd8e37c6u
      } },
    { "small300", 300u, 8192u, 9616u, 4u,
      { 2320u, 2928u, 3376u, 4800u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u },
      {
        0x3d8243d3u, 0x7b0487a6u, 0xf6090f4cu, 0x376318d9u,
        0x6ec631b2u, 0xdd8c6364u, 0x6069c089u, 0xc0d38112u,
        0x5ad60465u, 0xb5ac08cau, 0xb02917d5u, 0xbb2329ebu,
        0xad375597u, 0x811fad6fu, 0xd94e5c9fu, 0x69edbf7fu,
        0xd3db7efeu, 0x7cc7fbbdu, 0xf98ff77au, 0x286ee8b5u,
        0x50ddd16au, 0xa1bba2d4u, 0x980643e9u, 0xeb7d8193u,
        0x0d8a0567u, 0x1b140aceu, 0x3628159cu, 0x6c502b38u,
        0xd8a05670u, 0x6a31aaa1u, 0xd4635542u, 0x73b7acc5u
      } },
    { "small600", 600u, 16384u, 19216u, 4u,
      { 4640u, 5856u, 6752u, 9600u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u },
      {
        0xcc175950u, 0x435fb4e1u, 0x86bf69c2u, 0xd60fd5c5u,
        0x776eadcbu, 0xeedd5b96u, 0x06cbb16du, 0x0d9762dau,
        0x1b2ec5b4u, 0x365d8b68u, 0x6cbb16d0u, 0xd9762da0u,
        0x699d5d01u, 0xd33aba02u, 0x7d047245u, 0xfa08e48au,
        0x2f60cf55u, 0x5ec19eaau, 0xbd833d54u, 0xa0777ce9u,
        0x9b9fff93u, 0xec4ef967u, 0x03ecf48fu, 0x07d9e91eu,
        0x0fb3d23cu, 0x1f67a478u, 0x3ecf48f0u, 0x7d9e91e0u,
        0xfb3d23c0u, 0x2d0b41c1u, 0x5a168382u, 0xb42d0704u
      } },
    { "sparse4_3006", 3006u, 65536u, 96208u, 3u,
      { 2240u, 12656u, 48096u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u },
      {
        0xbe07fa84u, 0xa77ef349u, 0x958ce0d3u, 0xf068c7e7u,
        0x3ba0898fu, 0x7741131eu, 0xee82263cu, 0x06754a39u,
        0x0cea9472u, 0x19d528e4u, 0x33aa51c8u, 0x6754a390u,
        0xcea94720u, 0x46238801u, 0x8c471002u, 0xc3ff2645u,
        0x5c8f4acbu, 0xb91e9596u, 0xa94c2d6du, 0x89e95c9bu,
        0xc8a3bf77u, 0x4a3678afu, 0x946cf15eu, 0xf3a8e4fdu,
        0x3c20cfbbu, 0x78419f76u, 0xf0833eecu, 0x3a777b99u,
        0x74eef732u, 0xe9ddee64u, 0x08cada89u, 0x1195b512u
      } },
    { "dense4_5869", 5869u, 131072u, 187824u, 3u,
      { 544u, 768u, 93904u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u },
      {
        0x1fb16e8fu, 0x3f62dd1eu, 0x7ec5ba3cu, 0xfd8b7478u,
        0x2067eeb1u, 0x40cfdd62u, 0x819fbac4u, 0xd84e73c9u,
        0x6bede1d3u, 0xd7dbc3a6u, 0x74c6810du, 0xe98d021au,
        0x086b0275u, 0x10d604eau, 0x21ac09d4u, 0x435813a8u,
        0x86b02750u, 0xd61148e1u, 0x77539783u, 0xeea72f06u,
        0x063f584du, 0x0c7eb09au, 0x18fd6134u, 0x31fac268u,
        0x63f584d0u, 0xc7eb09a0u, 0x54a71501u, 0xa94e2a02u,
        0x89ed5245u, 0xc8aba2cbu, 0x4a2643d7u, 0x944c87aeu
      } },
    { "dense5_14870", 14870u, 262144u, 475856u, 4u,
      { 112u, 176u, 352u, 237920u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u },
      {
        0x7f9ee716u, 0xff3dce2cu, 0x250a9a19u, 0x4a153432u,
        0x942a6864u, 0xf325d689u, 0x3d3aab53u, 0x7a7556a6u,
        0xf4eaad4cu, 0x32a45cd9u, 0x6548b9b2u, 0xca917364u,
        0x4e53e089u, 0x9ca7c112u, 0xe23e8465u, 0x1f0c0e8bu,
        0x3e181d16u, 0x7c303a2cu, 0xf8607458u, 0x2bb1eef1u,
        0x5763dde2u, 0xaec7bbc4u, 0x86fe71c9u, 0xd68de5d3u,
        0x766acde7u, 0xecd59bceu, 0x02da31ddu, 0x05b463bau,
        0x0b68c774u, 0x16d18ee8u, 0x2da31dd0u, 0x5b463ba0u
      } },
    { "sparse3_91639", 91639u, 2097152u, 2932464u, 2u,
      { 799376u, 1466224u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u },
      {
        0x54bb1dd6u, 0xa9763bacu, 0x899d7119u, 0xc84be473u,
        0x4be6cea7u, 0x97cd9d4eu, 0xf4ea3cddu, 0x32a57ffbu,
        0x654afff6u, 0xca95ffecu, 0x4e5af999u, 0x9cb5f332u,
        0xe21ae025u, 0x1f44c60bu, 0x3e898c16u, 0x7d13182cu,
        0xfa263058u, 0x2f3d66f1u, 0x5e7acde2u, 0xbcf59bc4u,
        0xa29a31c9u, 0x9e4565d3u, 0xe7fbcde7u, 0x14869d8fu,
        0x290d3b1eu, 0x521a763cu, 0xa434ec78u, 0x9318deb1u,
        0xfd40bb23u, 0x21f07007u, 0x43e0e00eu, 0x87c1c01cu
      } }
};

/* At most 2 MiB, only in the benchmark binary's ordinary .bss.
 * No re-entrancy: the full EE test suite invokes candidates serially. */
static uint8_t poly_ring[2u * 1024u * 1024u] __attribute__((aligned(16)));

unsigned ps2_crc_poly_count(void) {
    return (unsigned)(sizeof(polynomials)/sizeof(polynomials[0]));
}
const char *ps2_crc_poly_name(unsigned id) {
    return id < ps2_crc_poly_count() ? polynomials[id].name : "invalid";
}
size_t ps2_crc_poly_minimum(unsigned id) {
    return id < ps2_crc_poly_count() ? polynomials[id].minimum : 0u;
}
unsigned ps2_crc_poly_terms(unsigned id) {
    return id < ps2_crc_poly_count() ? polynomials[id].tap_count + 1u : 0u;
}
unsigned ps2_crc_poly_degree(unsigned id) {
    return id < ps2_crc_poly_count() ? polynomials[id].degree : 0u;
}

/* The assembler operates on R5900 GPR 128-bit units, not SPR/DMA.
 * Testable host fallback below uses the exact same tap schedule. */
static inline void xor_scatter(uint8_t *dst, const uint8_t *value) {
#if defined(CRC_POLY_HOST_TEST)
    for (unsigned i = 0; i < 16; ++i) dst[i] ^= value[i];
#else
    __asm__ volatile (
        "lq $8, 0(%[val])\n\t"
        "lq $9, 0(%[dst])\n\t"
        "pxor $9, $9, $8\n\t"
        "sq $9, 0(%[dst])"
        :
        : [val] "r"(value), [dst] "r"(dst)
        : "$8", "$9", "memory"
    );
#endif
}
static inline void xor_scatter_pair(uint8_t *dst1, uint8_t *dst2,
                                     const uint8_t *value) {
#if defined(CRC_POLY_HOST_TEST)
    xor_scatter(dst1,value);
    xor_scatter(dst2,value);
#else
    __asm__ volatile (
        "lq $8, 0(%[val])\n\t"
        "lq $9, 0(%[a])\n\t"
        "lq $10, 0(%[b])\n\t"
        "pxor $9, $9, $8\n\t"
        "pxor $10, $10, $8\n\t"
        "sq $9, 0(%[a])\n\t"
        "sq $10, 0(%[b])"
        :
        : [val] "r"(value), [a] "r"(dst1), [b] "r"(dst2)
        : "$8", "$9", "$10", "memory"
    );
#endif
}
static uint32_t undo_shift(const crc_poly_spec *p, uint32_t value) {
    uint32_t result = 0u;
    for (unsigned bit=0; bit<32; ++bit)
        if (value & (1u << bit)) result ^= p->unshift[bit];
    return result;
}

/* Return a normal, public CRC32 value for any initial seed.
 * Only the caller-selected qualifying lengths execute the MMI kernel.
 * Original input is not modified. Large ring buffers stay out of the stack.
 * The residue is CRC'd directly from one/two ring segments, avoiding
 * a separate large temporary buffer and its copy overhead.
 */
uint32_t ps2_crc_poly_mmi(unsigned id, uint32_t crc,
                          const uint8_t *buf, size_t len) {
    if (id >= ps2_crc_poly_count()) return crc32_braid(crc,buf,len);
    const crc_poly_spec *p = &polynomials[id];
    size_t original_len = len;
    if (len < p->minimum) return crc32_braid(crc,buf,len);
    size_t align = (16u - ((uintptr_t)buf & 15u)) & 15u;
    if (align != 0u) {
        crc = crc32_braid(crc,buf,align);
        buf += align;
        len -= align;
    }
    if (len < p->minimum) return crc32_braid(crc,buf,len);
    const size_t processed = len & ~(size_t)15u;
    const size_t mask = p->ring_bytes - 1u;
    const size_t horizon = (size_t)p->degree * 16u;
    uint8_t first[16] __attribute__((aligned(16)));
    uint8_t value[16] __attribute__((aligned(16)));
    /* Do not clear beyond this polynomial's actual ring. */
    memset(poly_ring,0,p->ring_bytes);
    memcpy(first,buf,16);
    uint32_t seed = ~crc;
    for (unsigned j = 0; j < 4; ++j)
        first[j] ^= (uint8_t)(seed >> (j*8u));

    for (size_t i=0;i<processed;i+=16u) {
        uint8_t *slot = poly_ring + (i & mask);
        const uint8_t *src = i == 0u ? first : buf+i;
#if defined(CRC_POLY_HOST_TEST)
        for (unsigned j=0;j<16;++j) value[j]=src[j]^slot[j];
#else
        __asm__ volatile (
            "lq $8, 0(%[src])\n\t"
            "lq $9, 0(%[slot])\n\t"
            "pxor $8, $8, $9\n\t"
            "sq $8, 0(%[value])"
            :
            : [src] "r"(src), [slot] "r"(slot), [value] "r"(value)
            : "$8", "$9", "memory"
        );
#endif
        memset(slot,0,16);
        unsigned j=0;
        for (;j+1<p->tap_count;j+=2) {
            const size_t a=(i+p->taps[j])&mask;
            const size_t b=(i+p->taps[j+1])&mask;
            xor_scatter_pair(poly_ring+a,poly_ring+b,value);
        }
        if (j<p->tap_count) {
            const size_t a=(i+p->taps[j])&mask;
            xor_scatter(poly_ring+a,value);
        }
    }
    /* Account for the outstanding transformed stream, chronological order. */
    size_t start=processed&mask;
    size_t first_bytes=p->ring_bytes-start;
    if (first_bytes>horizon) first_bytes=horizon;
    uint32_t pending=crc32_braid(~0u,poly_ring+start,first_bytes);
    if (first_bytes<horizon)
        pending=crc32_braid(pending,poly_ring,horizon-first_bytes);
    uint32_t result=~undo_shift(p,~pending);
    if (processed<len)
        result=crc32_braid(result,buf+processed,len-processed);
    (void)original_len;
    return result;
}
