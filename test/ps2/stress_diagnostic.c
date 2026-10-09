#include "stress_diagnostic.h"
#include <stdarg.h>
#include <debug.h>
static char failure[256];
void mmi_stress_fail(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(failure, sizeof(failure), fmt, args);
    va_end(args);
    fputs(failure, stdout);
    fflush(stdout);
}
void ps2_stress_show_error(void) {
    if (!failure[0]) return;
    scr_setfontcolor(0x000000ffu);
    scr_printf("%s", failure);
    scr_setfontcolor(0x00ffffffu);
}
