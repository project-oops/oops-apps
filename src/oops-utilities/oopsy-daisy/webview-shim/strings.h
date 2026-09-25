/*
 * Freestanding <strings.h> for the webview build's C side (gumbo).
 *
 * gumbo's HTML parser uses the case-insensitive compares from <strings.h>, which the SDK's
 * freestanding libc does not provide. Implemented inline over ASCII case folding (gumbo compares
 * tag/attribute names, which are ASCII). Part of the webview freestanding-libc workaround
 * (REQ-20260925T0110Z-e1d9).
 */
#ifndef OOPSY_SHIM_STRINGS_H
#define OOPSY_SHIM_STRINGS_H

#include <stddef.h>

static inline int __oopsy_lc(int __c) { return (__c >= 'A' && __c <= 'Z') ? __c + 32 : __c; }

static inline int strcasecmp(const char *__a, const char *__b) {
    while (*__a && *__b) {
        int __d = __oopsy_lc((unsigned char)*__a) - __oopsy_lc((unsigned char)*__b);
        if (__d) return __d;
        __a++; __b++;
    }
    return __oopsy_lc((unsigned char)*__a) - __oopsy_lc((unsigned char)*__b);
}

static inline int strncasecmp(const char *__a, const char *__b, size_t __n) {
    for (size_t __i = 0; __i < __n; __i++) {
        int __d = __oopsy_lc((unsigned char)__a[__i]) - __oopsy_lc((unsigned char)__b[__i]);
        if (__d) return __d;
        if (!__a[__i]) return 0;
    }
    return 0;
}

#endif /* OOPSY_SHIM_STRINGS_H */
