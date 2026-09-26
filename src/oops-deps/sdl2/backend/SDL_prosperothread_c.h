/*
 * The platform thread handle, which `src/thread/SDL_thread_c.h` needs to lay out
 * `struct SDL_Thread`. The arm `patches/0001-*` adds there includes this by bare name,
 * through `-Ibackend`.
 */
#ifndef SDL_prosperothread_c_h_
#define SDL_prosperothread_c_h_

#include "oops/thread.h"

typedef oops_thread_t SYS_ThreadHandle;

#endif /* SDL_prosperothread_c_h_ */
