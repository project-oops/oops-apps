/*
 * Freestanding <fenv.h> for the webview build's C side (QuickJS).
 *
 * QuickJS sets round-to-nearest for its printf (CONFIG_PRINTF_RNDN). The hardware default is
 * already round-to-nearest, so fesetround is a no-op here; number formatting is unaffected for the
 * values a UI produces. Part of the webview freestanding-libc workaround (REQ-20260925T0110Z-e1d9).
 */
#ifndef OOPSY_SHIM_FENV_H
#define OOPSY_SHIM_FENV_H

#define FE_TONEAREST  0x0000
#define FE_DOWNWARD   0x0400
#define FE_UPWARD     0x0800
#define FE_TOWARDZERO 0x0c00

typedef int fenv_t;
typedef int fexcept_t;

static inline int fesetround(int __r) { (void)__r; return 0; }
static inline int fegetround(void)    { return FE_TONEAREST; }

#endif /* OOPSY_SHIM_FENV_H */
