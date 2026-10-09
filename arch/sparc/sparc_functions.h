/* sparc_functions.h -- SPARC implementations of generic entry points.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef SPARC_FUNCTIONS_H_
#define SPARC_FUNCTIONS_H_

#ifdef SPARC_VIS1
uint32_t adler32_vis1(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t adler32_copy_vis1(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#ifdef SPARC_VIS1_CHUNKSET
uint8_t *chunkmemset_safe_sparc(uint8_t *out, uint8_t *from, size_t len, size_t left);
#endif
uint32_t compare256_vis1(const uint8_t *src0, const uint8_t *src1);
uint32_t longest_match_vis1(deflate_state *const s, uint32_t cur_match);
uint32_t longest_match_slow_knuth_vis1(deflate_state *const s, uint32_t cur_match);
uint32_t longest_match_slow_roll_vis1(deflate_state *const s, uint32_t cur_match);
#endif

#ifdef SPARC_VIS1_SLIDEHASH
void slide_hash_vis1(deflate_state *s);
void slide_hash_head_vis1(deflate_state *s);
#endif

/* Keep generic variants for safe runtime dispatch and scalar primitives. */
#define ADLER32_FALLBACK
#define CHUNKSET_FALLBACK
#define COMPARE256_FALLBACK
#define CRC32_BRAID_FALLBACK
#define SLIDE_HASH_FALLBACK

/* With detection disabled, WITH_VIS1 is the user's promise that the
 * target CPU supports VIS1. */
#if defined(DISABLE_RUNTIME_CPU_DETECTION) && defined(SPARC_VIS1)
#  undef native_adler32
#  define native_adler32 adler32_vis1
#  undef native_adler32_copy
#  define native_adler32_copy adler32_copy_vis1
#  ifdef SPARC_VIS1_CHUNKSET
#    undef native_chunkmemset_safe
#    define native_chunkmemset_safe chunkmemset_safe_sparc
#  endif
#  undef native_compare256
#  define native_compare256 compare256_vis1
#  undef native_longest_match
#  define native_longest_match longest_match_vis1
#  undef native_longest_match_slow_knuth
#  define native_longest_match_slow_knuth longest_match_slow_knuth_vis1
#  undef native_longest_match_slow_roll
#  define native_longest_match_slow_roll longest_match_slow_roll_vis1
#endif

#if defined(DISABLE_RUNTIME_CPU_DETECTION) && defined(SPARC_VIS1_SLIDEHASH)
#  undef native_slide_hash
#  define native_slide_hash slide_hash_vis1
#  undef native_slide_hash_head
#  define native_slide_hash_head slide_hash_head_vis1
#endif

#endif /* SPARC_FUNCTIONS_H_ */
