/*
 * Freestanding <malloc_np.h> for the webview build's C side (QuickJS, FreeBSD path).
 *
 * QuickJS calls malloc_usable_size() only to feed its own memory-usage counter; the SDK heap does
 * not expose it, so this returns 0 (the documented "unknown size" fallback QuickJS already handles).
 * Allocation itself is unaffected. Part of the webview freestanding-libc workaround
 * (REQ-20260925T0110Z-e1d9).
 */
#ifndef OOPSY_SHIM_MALLOC_NP_H
#define OOPSY_SHIM_MALLOC_NP_H

#include <stddef.h>

static inline size_t malloc_usable_size(const void *__p) { (void)__p; return 0; }

#endif /* OOPSY_SHIM_MALLOC_NP_H */
