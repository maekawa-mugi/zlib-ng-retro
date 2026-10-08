/* mips_functions.h -- MIPS implementations for arch-specific functions.
 * Copyright (C) 2026 Mika T. Lindqvist <postmaster@raasu.org>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef MIPS_FUNCTIONS_H_
#define MIPS_FUNCTIONS_H_

#include "mips_natives.h"

#ifdef MIPS_MSA
uint32_t adler32_msa(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t adler32_copy_msa(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
void slide_hash_msa(deflate_state *s);
#endif

#ifdef MIPS_MMI_CHORBA
uint32_t crc32_chorba_mmi(uint32_t crc, const uint8_t *buf, size_t len);
uint32_t crc32_chorba_mmi_single(uint32_t crc, const uint8_t *buf, size_t len);
uint32_t crc32_chorba_mmi_paired(uint32_t crc, const uint8_t *buf, size_t len);
uint32_t crc32_copy_chorba_mmi(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len);
#endif

#ifdef MIPS_MMI_ADLER32
uint32_t adler32_mmi(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t adler32_copy_mmi(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#endif

#ifdef MIPS_MMI
void slide_hash_mmi(deflate_state *s);
void slide_hash_head_mmi(deflate_state *s);
/* Testable scheduling alternatives; default dispatch stays unchanged. */
void slide_hash_mmi_serial(deflate_state *s);
void slide_hash_mmi_interleaved(deflate_state *s);
void slide_hash_head_mmi_serial(deflate_state *s);
void slide_hash_head_mmi_interleaved(deflate_state *s);
uint32_t compare256_mmi_plain(const uint8_t *src0, const uint8_t *src1);
#ifdef MIPS_MMI_COMPARE64
uint32_t compare256_mmi_prefilter64(const uint8_t *src0, const uint8_t *src1);
#endif
uint8_t *chunkmemset_safe_mmi(uint8_t *out, uint8_t *from, size_t len, size_t left);
uint8_t *chunkmemset_safe_mmi_serial(uint8_t *out, uint8_t *from, size_t len, size_t left);
uint8_t *chunkmemset_safe_mmi_burst(uint8_t *out, uint8_t *from, size_t len, size_t left);
uint32_t compare256_mmi(const uint8_t *src0, const uint8_t *src1);
uint32_t longest_match_mmi(deflate_state *const s, uint32_t cur_match);
uint32_t longest_match_slow_knuth_mmi(deflate_state *const s, uint32_t cur_match);
uint32_t longest_match_slow_roll_mmi(deflate_state *const s, uint32_t cur_match);
#endif

#define CHUNKSET_FALLBACK
#define COMPARE256_FALLBACK
#define CRC32_BRAID_FALLBACK

#if !defined(MIPS_MSA_NATIVE)
#  define ADLER32_FALLBACK
#endif
#if !defined(MIPS_MSA_NATIVE) && !defined(MIPS_MMI_NATIVE)
#  define SLIDE_HASH_FALLBACK
#endif

#ifdef DISABLE_RUNTIME_CPU_DETECTION
// PS2 EE - MMI
#  if defined(MIPS_MMI_CHORBA) && defined(MIPS_MMI_NATIVE)
#    undef native_crc32
#    define native_crc32 crc32_chorba_mmi
#    undef native_crc32_copy
#    define native_crc32_copy crc32_copy_chorba_mmi
#  endif
#  ifdef MIPS_MMI_ADLER32_NATIVE
#    undef native_adler32
#    define native_adler32 adler32_mmi
#    undef native_adler32_copy
#    define native_adler32_copy adler32_copy_mmi
#  endif
#  ifdef MIPS_MMI_NATIVE
#    undef native_chunkmemset_safe
#    define native_chunkmemset_safe chunkmemset_safe_mmi
#    undef native_compare256
#    define native_compare256 compare256_mmi
#    undef native_longest_match
#    define native_longest_match longest_match_mmi
#    undef native_longest_match_slow_knuth
#    define native_longest_match_slow_knuth longest_match_slow_knuth_mmi
#    undef native_longest_match_slow_roll
#    define native_longest_match_slow_roll longest_match_slow_roll_mmi
#    undef native_slide_hash
#    define native_slide_hash slide_hash_mmi
#    undef native_slide_hash_head
#    define native_slide_hash_head slide_hash_head_mmi
#  endif

// MIPS - MSA
#  ifdef MIPS_MSA_NATIVE
#    undef native_adler32
#    define native_adler32 adler32_msa
#    undef native_adler32_copy
#    define native_adler32_copy adler32_copy_msa
#    undef native_slide_hash
#    define native_slide_hash slide_hash_msa
#  endif
#endif

#endif /* MIPS_FUNCTIONS_H_ */
