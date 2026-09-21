/* oops-sdk's <stdlib.h>, plus `aligned_alloc`.
 *
 * # Why this file exists rather than a change to oops-sdk
 *
 * libc++'s `__libcpp_aligned_alloc` calls `::aligned_alloc` whenever
 * `_LIBCPP_HAS_C11_ALIGNED_ALLOC` is on, which is the default. That path is reached by
 * `libcxxabi/src/fallback_malloc.cpp`, which is reached by `__cxa_allocate_exception` - so it is
 * on the road to every `throw`, and it is the one libc++abi source that will not compile without
 * it.
 *
 * oops-sdk's freestanding libc has `malloc`, `free`, `calloc` and `realloc` but no
 * `aligned_alloc`. Adding one there is a different repository's call, and would put a function in
 * the SDK that nothing in the SDK calls. So the dependency that needs it declares it, next to the
 * other headers it already carries for the same reason (`__config_site`, `_mbstate_t.h`).
 *
 * # The alternative that was rejected
 *
 * Setting `_LIBCPP_HAS_C11_ALIGNED_ALLOC 0` in `__config_site` sends libc++ down the
 * `posix_memalign` branch instead. That trades one absent function for another - the SDK has
 * neither - and `posix_memalign`'s out-parameter form is the more awkward of the two to
 * implement. It is the same amount of work in a less obvious place.
 *
 * # `#include_next`, and why the include order is load-bearing
 *
 * This header has the same name as the one it extends, so it must be found *first* and pull the
 * SDK's in behind it. That means both directories come in as `-isystem`, this one before
 * oops-sdk's, which is what `oops-libcxx.mk` arranges. A plain `-I` would not do: libc++'s own
 * `<cstdlib>` reaches the C library through `#include_next <stdlib.h>`, and `#include_next` walks
 * the *system* include chain. Putting the C library on `-I` is exactly how this failed the first
 * time, with `<cstdlib> tried including <stdlib.h>` against seventeen of libc++abi's nineteen
 * sources.
 */
#ifndef OOPS_LIBCXX_STDLIB_H
#define OOPS_LIBCXX_STDLIB_H

#include_next <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* C11 7.22.3.1. Defined by the exceptions runtime in `common/cxxrt.cpp`, over the SDK heap.
 * Declared here unconditionally: a title that never throws never links the object that calls it,
 * so a declaration costs nothing and a missing one costs the whole build. */
void *aligned_alloc(size_t alignment, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_LIBCXX_STDLIB_H */
