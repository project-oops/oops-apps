/*
 * Threads, mutexes and semaphores over `oops/thread.h`. Upstream's
 * `src/thread/generic/` builds the condition variable (`SDL_syscond.c`) and
 * thread-local storage (`SDL_systls.c`) on these. The generic mutex and semaphore are
 * built on each other, so the platform provides both.
 *
 * `SDL_LockMutex` is recursive by contract; `oops_mutex_init` is not, so the owner and
 * recursion count are kept here.
 */
#include "SDL_internal.h"

#ifdef SDL_THREAD_PROSPERO

#include "SDL_thread.h"
#include "SDL_timer.h"
#include "thread/SDL_thread_c.h"
#include "thread/SDL_systhread.h"

#include "oops/thread.h"

/* ---------------------------------------------------------------- threads */

/* `SDL_RunThread` runs the user function and records its return value. */
static void *PROSPERO_RunThread(void *data) {
    SDL_RunThread((SDL_Thread *)data);
    return NULL;
}

int SDL_SYS_CreateThread(SDL_Thread *thread) {
    oops_thread_t handle;

    handle = oops_thread_create(thread->name ? thread->name : "SDLThread",
                                PROSPERO_RunThread, thread, 0, thread->stacksize);
    if (!handle) {
        return SDL_SetError("prospero: the thread was not created");
    }
    thread->handle = handle;
    return 0;
}

void SDL_SYS_SetupThread(const char *name) {
    (void)name;
    /*
     * The SDK takes the name at creation. Empty but required: `SDL_RunThread` calls it
     * unconditionally.
     */
}

SDL_threadID SDL_ThreadID(void) {
    return (SDL_threadID)(uintptr_t)oops_thread_self();
}

int SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority) {
    (void)priority;
    /*
     * The SDK sets priority only at `oops_thread_create`, so changing it afterwards is
     * refused rather than silently ignored.
     */
    return SDL_Unsupported();
}

void SDL_SYS_WaitThread(SDL_Thread *thread) {
    if (thread->handle) {
        oops_thread_join(thread->handle, NULL);
        thread->handle = NULL;
    }
}

void SDL_SYS_DetachThread(SDL_Thread *thread) {
    if (thread->handle) {
        oops_thread_detach(thread->handle);
        thread->handle = NULL;
    }
}

/* ---------------------------------------------------------------- mutexes */

struct SDL_mutex {
    oops_mutex_t mutex;
    SDL_threadID owner;
    int recursion;
};

SDL_mutex *SDL_CreateMutex(void) {
    SDL_mutex *m = (SDL_mutex *)SDL_calloc(1, sizeof(*m));

    if (!m) {
        SDL_OutOfMemory();
        return NULL;
    }
    if (oops_mutex_init(&m->mutex, "SDLMutex") != 0) {
        SDL_free(m);
        SDL_SetError("prospero: the mutex was not created");
        return NULL;
    }
    return m;
}

void SDL_DestroyMutex(SDL_mutex *mutex) {
    if (mutex) {
        oops_mutex_destroy(&mutex->mutex);
        SDL_free(mutex);
    }
}

int SDL_LockMutex(SDL_mutex *mutex) {
    SDL_threadID self;

    if (!mutex) {
        return 0; /* SDL's documented no-op on a NULL mutex */
    }

    self = SDL_ThreadID();
    if (mutex->owner == self) {
        ++mutex->recursion;
        return 0;
    }
    if (oops_mutex_lock(&mutex->mutex) != 0) {
        return SDL_SetError("prospero: the mutex was not locked");
    }
    mutex->owner = self;
    mutex->recursion = 1;
    return 0;
}

int SDL_TryLockMutex(SDL_mutex *mutex) {
    SDL_threadID self;

    if (!mutex) {
        return 0;
    }

    self = SDL_ThreadID();
    if (mutex->owner == self) {
        ++mutex->recursion;
        return 0;
    }
    if (oops_mutex_trylock(&mutex->mutex) != 0) {
        return SDL_MUTEX_TIMEDOUT;
    }
    mutex->owner = self;
    mutex->recursion = 1;
    return 0;
}

int SDL_UnlockMutex(SDL_mutex *mutex) {
    if (!mutex) {
        return 0;
    }
    if (mutex->owner != SDL_ThreadID()) {
        return SDL_SetError("prospero: that mutex is not this thread's to unlock");
    }

    if (--mutex->recursion > 0) {
        return 0;
    }
    /* Cleared before the unlock, after which another thread may own it. */
    mutex->owner = 0;
    mutex->recursion = 0;
    if (oops_mutex_unlock(&mutex->mutex) != 0) {
        return SDL_SetError("prospero: the mutex was not unlocked");
    }
    return 0;
}

/* ------------------------------------------------------------- semaphores */

struct SDL_semaphore {
    oops_sem_t sem;
    SDL_atomic_t count;
};

/* The SDK's semaphore requires a maximum and SDL's has none; this is never reached. */
#define PROSPERO_SEM_MAX 0x7fffffff

SDL_sem *SDL_CreateSemaphore(Uint32 initial_value) {
    SDL_sem *s = (SDL_sem *)SDL_calloc(1, sizeof(*s));

    if (!s) {
        SDL_OutOfMemory();
        return NULL;
    }
    if (oops_sem_init(&s->sem, "SDLSem", (int)initial_value, PROSPERO_SEM_MAX) != 0) {
        SDL_free(s);
        SDL_SetError("prospero: the semaphore was not created");
        return NULL;
    }
    SDL_AtomicSet(&s->count, (int)initial_value);
    return s;
}

void SDL_DestroySemaphore(SDL_sem *sem) {
    if (sem) {
        oops_sem_destroy(&sem->sem);
        SDL_free(sem);
    }
}

int SDL_SemWait(SDL_sem *sem) {
    if (!sem) {
        return SDL_InvalidParamError("sem");
    }
    if (oops_sem_wait(&sem->sem, 1) != 0) {
        return SDL_SetError("prospero: the semaphore wait failed");
    }
    SDL_AtomicAdd(&sem->count, -1);
    return 0;
}

int SDL_SemTryWait(SDL_sem *sem) {
    if (!sem) {
        return SDL_InvalidParamError("sem");
    }
    if (oops_sem_poll(&sem->sem, 1) != 0) {
        return SDL_MUTEX_TIMEDOUT;
    }
    SDL_AtomicAdd(&sem->count, -1);
    return 0;
}

int SDL_SemWaitTimeout(SDL_sem *sem, Uint32 timeout) {
    if (!sem) {
        return SDL_InvalidParamError("sem");
    }
    if (timeout == SDL_MUTEX_MAXWAIT) {
        return SDL_SemWait(sem);
    }
    if (timeout == 0) {
        return SDL_SemTryWait(sem);
    }

    /*
     * The SDK has no timed semaphore wait, so a bounded wait polls once per
     * millisecond. A timed wait belongs in `oops/thread.h` if this ever matters.
     */
    {
        Uint64 deadline = SDL_GetTicks64() + timeout;

        for (;;) {
            if (oops_sem_poll(&sem->sem, 1) == 0) {
                SDL_AtomicAdd(&sem->count, -1);
                return 0;
            }
            if (SDL_GetTicks64() >= deadline) {
                return SDL_MUTEX_TIMEDOUT;
            }
            SDL_Delay(1);
        }
    }
}

int SDL_SemPost(SDL_sem *sem) {
    if (!sem) {
        return SDL_InvalidParamError("sem");
    }
    SDL_AtomicAdd(&sem->count, 1);
    if (oops_sem_signal(&sem->sem, 1) != 0) {
        SDL_AtomicAdd(&sem->count, -1);
        return SDL_SetError("prospero: the semaphore was not signalled");
    }
    return 0;
}

Uint32 SDL_SemValue(SDL_sem *sem) {
    int v;

    if (!sem) {
        return 0;
    }
    /*
     * Counted here because the SDK's semaphore does not report its value. SDL documents
     * the value as advisory.
     */
    v = SDL_AtomicGet(&sem->count);
    return v > 0 ? (Uint32)v : 0;
}

#endif /* SDL_THREAD_PROSPERO */
