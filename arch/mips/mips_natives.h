/* mips_natives.h -- MIPS compile-time feature detection macros.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef MIPS_NATIVES_H_
#define MIPS_NATIVES_H_

#if defined(__mips_msa)
#  ifdef MIPS_MSA
#    define MIPS_MSA_NATIVE
#  endif
#endif

/* MMI is an explicitly chosen fixed-CPU target; there is no MSA-style
 * runtime capability probe for PlayStation 2 EE. */
#if defined(MIPS_MMI) && defined(DISABLE_RUNTIME_CPU_DETECTION)
#  define MIPS_MMI_NATIVE
#endif

#endif /* MIPS_NATIVES_H_ */
