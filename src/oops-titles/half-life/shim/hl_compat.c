/*
 * `shim/include/hl_compat.h` says why these are here.
 */
#include "hl_compat.h"

#include <stdarg.h>

/* One line, scanned. `EOF` when there is no line to read, as `fscanf` reports end of file. */
int fscanf(FILE *f, const char *fmt, ...) {
    char line[512];
    va_list args;
    int n;

    if (!fgets(line, (int)sizeof(line), f)) {
        return EOF;
    }
    va_start(args, fmt);
    n = vsscanf(line, fmt, args);
    va_end(args);
    return n;
}
