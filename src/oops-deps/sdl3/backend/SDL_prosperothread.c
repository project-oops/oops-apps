/*
 * SDL3's thread, mutex and semaphore backend, over `oops/thread.h`.
 *
 * # SDL3 turned the threading relationship round, and this is the half that changed
 *
 * In SDL2 a platform supplied threads and the *generic* mutex and semaphore were built
 * on each other - which is why `oops-deps/sdl2` has a note about the two being circular
 * and a platform having to provide both. SDL3 asks the platform for the mutex and the
 * semaphore as **public API**
 * (`SDL_CreateMutex`, `SDL_WaitSemaphoreTimeoutNS`, not `SDL_SYS_*`), and builds its
 * condition variable, read-write lock and thread-local storage on top of those.
 * `src/thread/generic/` keeps `SDL_syscond.c`, `SDL_sysrwlock.c` and `SDL_systls.c`,
 * and this file replaces the other three - which is exactly the `filter-out` list in
 * `oops-sdl3.mk`.
 *
 * So everything here is a mapping onto something the SDK already has, and none of it is
 * a stub.
 *
 * # The mutex is recursive, because SDL says so
 *
 * `SDL_CreateMutex`'s documentation: "This mutex is recursive, which means a thread
 * that locks it can lock it again without blocking." `oops_mutex_init_recursive` is the
 * one to call, and calling plain `oops_mutex_init` instead would deadlock a caller that
 * relocks - the kind of fault that appears once, deep inside somebody else's library,
 * under load.
 *
 * # Where a failure goes
 *
 * SDL's own backends split two ways and this follows them: an allocation or creation
 * failure returns NULL with `SDL_SetError`, and a *lock* failure is an assertion,
 * because the caller has no way to recover and SDL's callers do not check. The Vita
 * backend's comment says it plainly - "assume we're in a lot of trouble if this assert
 * fails".
 */
#include "SDL_internal.h"

#ifdef SDL_THREAD_PRIVATE

/* Reached through `-I<upstream>/src`: SDL's own backends sit inside the tree and write
 * `../`, this one does not. */
#include "thread/SDL_systhread.h"
#include "thread/SDL_thread_c.h"

#include "oops/thread.h"
#include "oops/time.h" /* oops_time_sleep_us, for the bounded semaphore wait below */

/* ---- threads ------------------------------------------------------------ */

/*
 * `oops_thread_create` takes `void *(*)(void *)` and SDL's runner returns nothing, so
 * the entry is wrapped rather than cast. A cast would work on this ABI and is the kind
 * of thing that stops working quietly.
 */
static void *oops_sdl_thread_entry(void *arg) {
    SDL_RunThread((SDL_Thread *)arg);
    return NULL;
}

bool SDL_SYS_CreateThread(SDL_Thread *thread, SDL_FunctionPointer pfnBeginThread,
                          SDL_FunctionPointer pfnEndThread) {
    /* Windows' `_beginthreadex` pair. Nothing to do here, and SDL passes NULL for both
     * on every platform that is not Windows. */
    (void)pfnBeginThread;
    (void)pfnEndThread;

    /* `0` for the priority is the SDK's "leave it alone" - `thread.c:78` only builds a
     * pthread attribute set when the priority or the stack size is non-zero. SDL has no
     * priority to offer at creation: `SDL_SetCurrentThreadPriority` is called from
     * *inside* the new thread, which is what `SDL_SYS_SetThreadPriority` below is for.
     * Likewise `stacksize` 0 means the default. */
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
    /* The name is given at creation - `oops_thread_create` takes it - and there is no
     * call to rename a running thread. Nothing to do, which is also what the Vita and
     * PSP backends say. */
    (void)name;
}

SDL_ThreadID SDL_GetCurrentThreadID(void) {
    return (SDL_ThreadID)(uintptr_t)oops_thread_self();
}

bool SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority) {
    /*
     * **Refused rather than ignored.** `oops/thread.h` sets a thread's priority through
     * the attributes it is *created* with (`scePthreadAttrSetprio`); there is no call
     * to change one that is already running, which is what this asks for.
     *
     * Returning true would tell a caller its real-time audio thread had been promoted
     * when nothing had happened. SDL treats the failure as advisory -
     * `SDL_SetThreadPriority` returns it to the caller and carries on - so a program
     * that asks gets the truth and still runs.
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

/* ---- mutexes ------------------------------------------------------------ */

struct SDL_Mutex {
    oops_mutex_t lock;
};

SDL_Mutex *SDL_CreateMutex(void) {
    SDL_Mutex *mutex = (SDL_Mutex *)SDL_calloc(1, sizeof(*mutex));
    if (!mutex) {
        return NULL; /* SDL_calloc has already set the error */
    }
    /* Recursive, because SDL_CreateMutex promises it. See the note at the top. */
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

/* ---- semaphores --------------------------------------------------------- */

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
 * The three cases SDL asks for, and the third is the one with a compromise in it.
 *
 *   timeoutNS < 0   wait forever      -> `oops_sem_wait`, which blocks
 *   timeoutNS == 0  poll              -> `oops_sem_poll`, which does not
 *   timeoutNS > 0   wait this long    -> **polled**, because the SDK has no timed wait
 *
 * `oops/thread.h` offers `oops_cond_timedwait` but no timed semaphore wait, so a
 * bounded wait is a poll in a sleep loop. The cost is stated rather than hidden: the
 * wait is granular to the poll interval below, so a caller asking for 3ms may get up to
 * 4ms, and a signal arriving just after a poll waits out the rest of that interval
 * before being seen. That is latency, not a lost wakeup - the count is held by the
 * semaphore and the next poll takes it.
 *
 * 500us rather than a millisecond because SDL's own users of a bounded wait are audio
 * and event plumbing, where a millisecond of added latency is audible and half of one
 * is the compromise.
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
