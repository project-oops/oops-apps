/*
 * Threads, mutexes and semaphores over `oops/thread.h`.
 *
 * # Three of the five, and upstream writes the other two
 *
 * SDL's thread layer is thread, mutex, semaphore, condition variable and thread-local storage.
 * Only the first three are here, because upstream's `src/thread/generic/` already builds the
 * other two out of them - `SDL_syscond.c` from a mutex and a semaphore, `SDL_systls.c` from a
 * mutex - and those are real implementations rather than the stubs the same directory keeps for
 * platforms with no threads at all.
 *
 * **Which three is not a free choice.** Generic mutex is built on a semaphore and generic
 * semaphore is built on a mutex and a condition variable, so the two are circular and a platform
 * must break the ring by providing one of them. Providing both, as here, is what lets the
 * generic condition variable be reused rather than written a third time - and both map one to
 * one onto the SDK, so neither costs anything to provide.
 *
 * # Mutexes here are recursive
 *
 * `SDL_LockMutex` is documented to be recursive: the owning thread may lock again and must
 * unlock the same number of times. `oops_mutex_init` is not told to be, so the count is kept
 * here. Getting this wrong deadlocks a title on its own second lock rather than failing
 * anywhere visible, which is why it is the one part of this file with state of its own.
 */
#include "SDL_internal.h"

#ifdef SDL_THREAD_PROSPERO

#include "SDL_thread.h"
#include "SDL_timer.h"
#include "thread/SDL_thread_c.h"
#include "thread/SDL_systhread.h"

#include "oops/thread.h"

/* ---------------------------------------------------------------- threads */

/*
 * SDL's entry point returns void*, and `SDL_RunThread` is what actually runs the user function
 * and records its return value, so nothing here needs to interpret it.
 */
static void *PROSPERO_RunThread(void *data)
{
    SDL_RunThread((SDL_Thread *)data);
    return NULL;
}

int SDL_SYS_CreateThread(SDL_Thread *thread)
{
    oops_thread_t handle;

    handle = oops_thread_create(thread->name ? thread->name : "SDLThread",
                                PROSPERO_RunThread, thread, 0, thread->stacksize);
    if (!handle) {
        return SDL_SetError("prospero: the thread was not created");
    }
    thread->handle = handle;
    return 0;
}

void SDL_SYS_SetupThread(const char *name)
{
    (void)name;
    /*
     * The name is given at creation, which is the only point this SDK takes one, so there is
     * nothing left to do in the new thread itself. Deliberately empty rather than missing:
     * `SDL_RunThread` calls this unconditionally.
     */
}

SDL_threadID SDL_ThreadID(void)
{
    return (SDL_threadID)(uintptr_t)oops_thread_self();
}

int SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority)
{
    (void)priority;
    /*
     * Refused rather than ignored. `oops_thread_create` takes a priority and this SDK has no
     * call to change one afterwards, so pretending would leave a title believing its audio
     * thread had been raised. A title that must have the priority can ask for it at creation.
     */
    return SDL_Unsupported();
}

void SDL_SYS_WaitThread(SDL_Thread *thread)
{
    if (thread->handle) {
        oops_thread_join(thread->handle, NULL);
        thread->handle = NULL;
    }
}

void SDL_SYS_DetachThread(SDL_Thread *thread)
{
    if (thread->handle) {
        oops_thread_detach(thread->handle);
        thread->handle = NULL;
    }
}

/* ---------------------------------------------------------------- mutexes */

struct SDL_mutex
{
    oops_mutex_t mutex;
    SDL_threadID owner;
    int recursion;
};

SDL_mutex *SDL_CreateMutex(void)
{
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

void SDL_DestroyMutex(SDL_mutex *mutex)
{
    if (mutex) {
        oops_mutex_destroy(&mutex->mutex);
        SDL_free(mutex);
    }
}

int SDL_LockMutex(SDL_mutex *mutex)
{
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

int SDL_TryLockMutex(SDL_mutex *mutex)
{
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

int SDL_UnlockMutex(SDL_mutex *mutex)
{
    if (!mutex) {
        return 0;
    }
    if (mutex->owner != SDL_ThreadID()) {
        return SDL_SetError("prospero: that mutex is not this thread's to unlock");
    }

    if (--mutex->recursion > 0) {
        return 0;
    }
    /* Cleared before the unlock, because after it another thread may already own this. */
    mutex->owner = 0;
    mutex->recursion = 0;
    if (oops_mutex_unlock(&mutex->mutex) != 0) {
        return SDL_SetError("prospero: the mutex was not unlocked");
    }
    return 0;
}

/* ------------------------------------------------------------- semaphores */

struct SDL_semaphore
{
    oops_sem_t sem;
    SDL_atomic_t count;
};

/*
 * The SDK's semaphore is created with a maximum, and SDL's has none. This is the ceiling a
 * counting semaphore is given here: high enough that no SDL use reaches it, and a number rather
 * than a guess at "unbounded", because the call requires one.
 */
#define PROSPERO_SEM_MAX 0x7fffffff

SDL_sem *SDL_CreateSemaphore(Uint32 initial_value)
{
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

void SDL_DestroySemaphore(SDL_sem *sem)
{
    if (sem) {
        oops_sem_destroy(&sem->sem);
        SDL_free(sem);
    }
}

int SDL_SemWait(SDL_sem *sem)
{
    if (!sem) {
        return SDL_InvalidParamError("sem");
    }
    if (oops_sem_wait(&sem->sem, 1) != 0) {
        return SDL_SetError("prospero: the semaphore wait failed");
    }
    SDL_AtomicAdd(&sem->count, -1);
    return 0;
}

int SDL_SemTryWait(SDL_sem *sem)
{
    if (!sem) {
        return SDL_InvalidParamError("sem");
    }
    if (oops_sem_poll(&sem->sem, 1) != 0) {
        return SDL_MUTEX_TIMEDOUT;
    }
    SDL_AtomicAdd(&sem->count, -1);
    return 0;
}

int SDL_SemWaitTimeout(SDL_sem *sem, Uint32 timeout)
{
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
     * **Polled, and that is a compromise worth naming.** The SDK has no timed semaphore wait -
     * `oops_cond_timedwait` is the only timed primitive - so a bounded wait is a poll and a
     * sleep. The cost is up to a millisecond of latency and a wakeup per millisecond while
     * waiting; the alternative is to rebuild the semaphore on a condition variable, which is
     * the generic implementation this file exists to avoid.
     *
     * If a title turns out to sit in a timed wait hot enough for this to matter, the answer is
     * a timed wait in `oops/thread.h`, not a cleverer loop here.
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

int SDL_SemPost(SDL_sem *sem)
{
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

Uint32 SDL_SemValue(SDL_sem *sem)
{
    int v;

    if (!sem) {
        return 0;
    }
    /*
     * Counted here because the SDK's semaphore does not report its value. It is advisory either
     * way - by the time a caller reads it another thread may have changed it - which is why SDL
     * documents this call as a hint rather than a synchronisation primitive.
     */
    v = SDL_AtomicGet(&sem->count);
    return v > 0 ? (Uint32)v : 0;
}

#endif /* SDL_THREAD_PROSPERO */
