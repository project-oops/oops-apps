/*
 * SDL3's thread, mutex and semaphore backend, over `oops/thread.h`.
 *
 * SDL3 takes the mutex and semaphore from the platform as public API
 * (`SDL_CreateMutex`, `SDL_WaitSemaphoreTimeoutNS`) and builds its condition variable,
 * read-write lock and thread-local storage in `src/thread/generic/` on top of them.
 * This file replaces the generic sources in `oops-sdl3.mk`'s `OOPS_SDL3_THREAD_DROP`.
 *
 * The mutex is recursive, as `SDL_CreateMutex` documents. As in SDL's own backends, a
 * creation failure returns NULL with `SDL_SetError` and a lock failure is an assertion.
 */
#include "SDL_internal.h"

#ifdef SDL_THREAD_PRIVATE

/* Reached through `-I<upstream>/src`; this file sits outside upstream's tree. */
#include "thread/SDL_systhread.h"
#include "thread/SDL_thread_c.h"

#include "oops/thread.h"
#include "oops/time.h" /* oops_time_sleep_us, for the bounded semaphore wait below */

/* Threads. */

/* `oops_thread_create` takes `void *(*)(void *)` and SDL's runner returns nothing, so
 * the entry is wrapped rather than cast. */
static void *oops_sdl_thread_entry(void *arg) {
    SDL_RunThread((SDL_Thread *)arg);
    return NULL;
}

bool SDL_SYS_CreateThread(SDL_Thread *thread, SDL_FunctionPointer pfnBeginThread,
                          SDL_FunctionPointer pfnEndThread) {
    /* Windows' `_beginthreadex` pair; NULL on every other platform. */
    (void)pfnBeginThread;
    (void)pfnEndThread;

    /* Priority 0 and stack size 0 are the SDK's defaults (`thread.c:78`). SDL sets
     * priority from inside the new thread, through `SDL_SYS_SetThreadPriority`. */
    thread->handle =
        oops_thread_create(thread->name ? thread->name : "SDLThread",
                           oops_sdl_thread_entry, thread, 0, thread->stacksize);
    if (!thread->handle) {
        return SDL_SetError("oops_thread_create() failed");
    }
    thread->threadid = (SDL_ThreadID)(uintptr_t)thread->handle;
    return true;
}

void SDL_SYS_SetupThread(const char *name) {
    /* The name is given to `oops_thread_create`; a running thread cannot be renamed. */
    (void)name;
}

SDL_ThreadID SDL_GetCurrentThreadID(void) {
    return (SDL_ThreadID)(uintptr_t)oops_thread_self();
}

bool SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority) {
    /*
     * `oops/thread.h` sets priority only through creation attributes
     * (`scePthreadAttrSetprio`), so changing a running thread is unsupported. SDL
     * returns the failure to the caller as advisory.
     */
    (void)priority;
    return SDL_Unsupported();
}

void SDL_SYS_WaitThread(SDL_Thread *thread) {
    if (thread && thread->handle) {
        (void)oops_thread_join(thread->handle, NULL);
    }
}

void SDL_SYS_DetachThread(SDL_Thread *thread) {
    if (thread && thread->handle) {
        (void)oops_thread_detach(thread->handle);
    }
}

/* Mutexes. */

struct SDL_Mutex {
    oops_mutex_t lock;
};

SDL_Mutex *SDL_CreateMutex(void) {
    SDL_Mutex *mutex = (SDL_Mutex *)SDL_calloc(1, sizeof(*mutex));
    if (!mutex) {
        return NULL; /* SDL_calloc has already set the error */
    }
    /* Recursive, as `SDL_CreateMutex` documents. */
    if (oops_mutex_init_recursive(&mutex->lock, "SDL") != 0) {
        SDL_free(mutex);
        SDL_SetError("oops_mutex_init_recursive() failed");
        return NULL;
    }
    return mutex;
}

void SDL_DestroyMutex(SDL_Mutex *mutex) {
    if (mutex) {
        (void)oops_mutex_destroy(&mutex->lock);
        SDL_free(mutex);
    }
}

/* A NULL mutex is a no-op throughout SDL's API, which clang's thread-safety analysis
 * cannot see. */
void SDL_LockMutex(SDL_Mutex *mutex) SDL_NO_THREAD_SAFETY_ANALYSIS {
    if (mutex) {
        const int rc = oops_mutex_lock(&mutex->lock);
        SDL_assert(rc == 0); /* nothing above this can recover from a failed lock */
        (void)rc;
    }
}

bool SDL_TryLockMutex(SDL_Mutex *mutex) SDL_NO_THREAD_SAFETY_ANALYSIS {
    if (!mutex) {
        return true; /* nothing to contend for */
    }
    return oops_mutex_trylock(&mutex->lock) == 0;
}

void SDL_UnlockMutex(SDL_Mutex *mutex) SDL_NO_THREAD_SAFETY_ANALYSIS {
    if (mutex) {
        const int rc = oops_mutex_unlock(&mutex->lock);
        SDL_assert(rc == 0);
        (void)rc;
    }
}

/* Semaphores. */

struct SDL_Semaphore {
    oops_sem_t sem;
};

SDL_Semaphore *SDL_CreateSemaphore(Uint32 initial_value) {
    SDL_Semaphore *sem = (SDL_Semaphore *)SDL_calloc(1, sizeof(*sem));
    if (!sem) {
        return NULL;
    }
    /* `0` is the SDK's "no ceiling"; SDL's semaphores have no maximum either. */
    if (oops_sem_init(&sem->sem, "SDL", (int)initial_value, 0) != 0) {
        SDL_free(sem);
        SDL_SetError("oops_sem_init() failed");
        return NULL;
    }
    return sem;
}

void SDL_DestroySemaphore(SDL_Semaphore *sem) {
    if (sem) {
        (void)oops_sem_destroy(&sem->sem);
        SDL_free(sem);
    }
}

void SDL_SignalSemaphore(SDL_Semaphore *sem) {
    if (sem) {
        (void)oops_sem_signal(&sem->sem, 1);
    }
}

/*
 * A negative timeout blocks in `oops_sem_wait` and zero polls once. `oops/thread.h`
 * has no timed semaphore wait, so a positive timeout polls in a sleep loop: a signal
 * is seen up to one interval late, never lost. The interval is short because SDL's
 * bounded waits are audio and event plumbing.
 */
#define OOPS_SEM_POLL_US 500u

bool SDL_WaitSemaphoreTimeoutNS(SDL_Semaphore *sem, Sint64 timeoutNS) {
    if (!sem) {
        return true;
    }
    if (timeoutNS < 0) {
        return oops_sem_wait(&sem->sem, 1) == 0;
    }
    if (timeoutNS == 0) {
        return oops_sem_poll(&sem->sem, 1) == 0;
    }
    {
        Uint64 remaining_ns = (Uint64)timeoutNS;
        for (;;) {
            if (oops_sem_poll(&sem->sem, 1) == 0) {
                return true;
            }
            if (remaining_ns == 0u) {
                return false;
            }
            {
                Uint64 step_ns = (Uint64)OOPS_SEM_POLL_US * 1000u;
                if (step_ns > remaining_ns) {
                    step_ns = remaining_ns;
                }
                oops_time_sleep_us((uint32_t)(step_ns / 1000u ? step_ns / 1000u : 1u));
                remaining_ns -= step_ns;
            }
        }
    }
}

#endif /* SDL_THREAD_PRIVATE */
