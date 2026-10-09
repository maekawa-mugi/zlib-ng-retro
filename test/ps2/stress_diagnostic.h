#ifndef PS2_STRESS_DIAGNOSTIC_H
#define PS2_STRESS_DIAGNOSTIC_H
#include <stdio.h>
#ifdef PS2_STRESS_DIAGNOSTIC
void mmi_stress_fail(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void ps2_stress_show_error(void);
#else
#define mmi_stress_fail printf
#endif
#endif
