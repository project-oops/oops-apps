/*
 * Freestanding libc++ shim for the webview build.
 *
 * OOPSy-DAISY is the first *freestanding* title to link libc++ (gl-cts is hosted, so it takes the
 * Mesa sysroot's C library). Freestanding libc++'s <stdlib.h> wrapper references `::lldiv`, which
 * the SDK's own freestanding <stdlib.h> (include/libc) does not declare yet - it has div/ldiv but
 * not the long-long pair. This header is force-included (-include) ahead of libc++ so that name
 * exists in the global namespace when libc++ pulls it into std::. It deliberately does NOT touch
 * ldiv_t/ldiv, which the SDK libc already provides (redefining them collides).
 *
 * This is a workaround for a gap in the SDK's freestanding libc, filed as REQ-20260925T0110Z-e1d9
 * in C:\tmp\OopsSdk\worklog.md; delete it once include/libc/stdlib.h grows lldiv.
 */
#ifndef OOPSY_WEBVIEW_LIBCXX_SHIM_H
#define OOPSY_WEBVIEW_LIBCXX_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { long long quot; long long rem; } lldiv_t;

lldiv_t lldiv(long long, long long);

#ifdef __cplusplus
}
#endif

#endif /* OOPSY_WEBVIEW_LIBCXX_SHIM_H */
