/*
 * What `struct SDL_Thread` holds for this platform.
 *
 * `SDL_thread_c.h` includes this to lay out the structure, so `backend/` is on the
 * include path. `patches/0001-*` adds the `SDL_THREAD_PRIVATE` arm that reaches it,
 * which SDL3's config chain does not have.
 *
 * `oops_thread_t` is already an opaque pointer-sized handle.
 */
#include "SDL_internal.h"

#include "oops/thread.h"

typedef oops_thread_t SYS_ThreadHandle;
