/*
 * The platform thread handle, which `src/thread/SDL_thread_c.h` needs before it can lay out
 * `struct SDL_Thread`. Reached through `-Ibackend` rather than a path inside upstream's tree,
 * because the arm that `patches/0001-*` adds to that file includes it by bare name.
 */
#ifndef SDL_prosperothread_c_h_
#define SDL_prosperothread_c_h_

#include "oops/thread.h"

typedef oops_thread_t SYS_ThreadHandle;

#endif /* SDL_prosperothread_c_h_ */
