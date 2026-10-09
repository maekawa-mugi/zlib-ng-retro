/* sparc_features.c -- SPARC VIS1 runtime detection for Linux.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif
#include "zbuild.h"
#include "arch/sparc/sparc_features.h"

#if defined(HAVE_SPARC_GETAUXVAL)
#  include <sys/auxv.h>
#  include <elf.h>
#endif

void Z_INTERNAL sparc_check_features(struct sparc_cpu_features *features) {
    features->has_vis1 = 0;
#if defined(HAVE_SPARC_GETAUXVAL)
    features->has_vis1 = sparc_hwcap_has_vis1(getauxval(AT_HWCAP));
#endif
}
