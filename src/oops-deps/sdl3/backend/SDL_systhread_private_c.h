/*
 * What `struct SDL_Thread` holds for this platform.
 *
 * `SDL_thread_c.h` includes this to lay out the structure, which is why it is a header
 * rather than something the backend keeps to itself - and why `backend/` has to be on
 * the include path. `patches/0001-*` adds the arm that reaches it; SDL3 offers
 * `SDL_THREAD_PRIVATE` in its generated config and then does not handle it in that
 * chain, which is the one gap in an otherwise complete private-platform story.
 *
 * `oops_thread_t` is already an opaque pointer-sized handle, so there is nothing to
 * wrap.
 */
#include "SDL_internal.h"

#include "oops/thread.h"

typedef oops_thread_t SYS_ThreadHandle;
