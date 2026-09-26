/*
 * <malloc.h>, which is not a standard header: the allocator is declared in <stdlib.h>, and that is
 * where this forwards. Linux and the N64 SDK both provide it, so a decompiled codebase includes it.
 *
 * `memalign` is the pre-C11 spelling of `aligned_alloc` with its arguments in the same order and
 * no requirement that the size be a multiple of the alignment. The SDK's `aligned_alloc` follows
 * C17 and does require that, so this rounds up rather than passing the request through.
 */
#ifndef SOH_SHIM_MALLOC_H
#define SOH_SHIM_MALLOC_H

#include <stdlib.h>

static inline void *memalign(size_t alignment, size_t size) {
    size_t rounded;

    if (alignment == 0) {
        return NULL;
    }
    rounded = (size + alignment - 1) / alignment * alignment;
    return aligned_alloc(alignment, rounded);
}

#endif /* SOH_SHIM_MALLOC_H */
