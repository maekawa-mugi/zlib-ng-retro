/* sparc_features.h -- SPARC runtime CPU feature detection.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef SPARC_FEATURES_H_
#define SPARC_FEATURES_H_

/* Linux/SPARC AT_HWCAP: VIS1 (0x00002000) is distinct from
 * BLKINIT (0x00000040). Keep CPU dispatch and direct-ISA test guards
 * synchronized through one constant. */
#define ZNG_SPARC_HWCAP_VIS 0x00002000UL

static inline int sparc_hwcap_has_vis1(unsigned long hwcap) {
    return (hwcap & ZNG_SPARC_HWCAP_VIS) != 0UL;
}

struct sparc_cpu_features {
    int has_vis1;
};

void sparc_check_features(struct sparc_cpu_features *features);

#endif /* SPARC_FEATURES_H_ */
